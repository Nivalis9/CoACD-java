package io.github.nivalis9.coacd;

import java.net.URL;
import java.net.URLClassLoader;
import java.util.concurrent.CancellationException;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicReference;

/** Minimal native smoke test; run with assertions enabled. */
public final class CoacdNativeSmokeTest {
    private CoacdNativeSmokeTest() {}

    public static void main(String[] args) {
        verifyJavaValidation();
        verifyNativeLoadRetry();
        verifyNativeExceptionRecovery();

        double[] vertices = {
                0, 0, 0,
                1, 0, 0,
                0, 1, 0,
                0, 0, 1
        };
        int[] triangles = {
                0, 2, 1,
                0, 1, 3,
                1, 2, 3,
                2, 0, 3
        };
        CoacdNative.Parameters parameters = new CoacdNative.Parameters(
                0.05,
                -1,
                CoacdNative.PreprocessMode.OFF,
                50,
                100,
                20,
                10,
                3,
                false,
                true,
                false,
                256,
                false,
                0.01,
                CoacdNative.ApproximationMode.CONVEX_HULL,
                0,
                false);
        if (parameters.getSampleResolution() != 100 || parameters.getMctsIterations() != 10) {
            throw new AssertionError("explicit parameter values were not retained");
        }
        CoacdNative.Mesh[] result = CoacdNative.decompose(
                new CoacdNative.Mesh(vertices, triangles), parameters);
        if (result.length == 0) {
            throw new AssertionError("CoACD returned no convex parts");
        }
        for (CoacdNative.Mesh part : result) {
            if (part.getVertexCount() == 0 || part.getTriangleCount() == 0) {
                throw new AssertionError("CoACD returned an empty part");
            }
        }
        verifyInterruption(parameters);
        verifyInFlightInterruption();
        verifyConcurrentCalls(parameters);
        verifyIsolatedClassLoaderLoad();
        System.out.println("CoACD JNI smoke test passed with " + result.length + " part(s)");
    }

    private static void verifyJavaValidation() {
        expectException(IllegalArgumentException.class,
                () -> new CoacdNative.Mesh(new double[9], new int[] {0, 1, 2}));
        expectException(IllegalArgumentException.class,
                () -> new CoacdNative.Mesh(
                        new double[] {-Double.MAX_VALUE, 0, 0, Double.MAX_VALUE, 0, 0, 0, 1, 0},
                        new int[] {0, 1, 2}));
        expectException(IllegalArgumentException.class,
                () -> new CoacdNative.Mesh(
                        new double[] {0, 0, 0, 1, 0, 0, 0, 1, 0},
                        new int[] {0, 1, 3}));
        expectException(IllegalArgumentException.class,
                () -> new CoacdNative.Mesh(
                        new double[] {0, 0, 0, 1, 0, 0, 0, 1, 0},
                        new int[] {0, 1, 1}));
        expectException(IllegalArgumentException.class,
                () -> new CoacdNative.Mesh(
                        new double[] {0, 0, 0, 1, 0, 0, 2, 0, 0},
                        new int[] {0, 1, 2}));
        expectException(IllegalArgumentException.class,
                () -> CoacdNative.Parameters.builder().seed(0x1_0000_0000L).build());
        expectException(IllegalArgumentException.class,
                () -> CoacdNative.setLogLevel("verbose"));
    }

    private static void verifyInterruption(CoacdNative.Parameters parameters) {
        Thread.currentThread().interrupt();
        try {
            expectException(CancellationException.class,
                    () -> CoacdNative.decompose(tetrahedron(), parameters));
            if (!Thread.currentThread().isInterrupted()) {
                throw new AssertionError("native cancellation cleared the Java interrupt flag");
            }
        } finally {
            Thread.interrupted();
        }
    }

    private static void verifyConcurrentCalls(CoacdNative.Parameters parameters) {
        AtomicReference<Throwable> failure = new AtomicReference<>();
        CountDownLatch start = new CountDownLatch(1);
        Runnable task = () -> {
            try {
                start.await();
                if (CoacdNative.decompose(tetrahedron(), parameters).length == 0) {
                    throw new AssertionError("concurrent decomposition returned no parts");
                }
            } catch (Throwable error) {
                failure.compareAndSet(null, error);
            }
        };
        Thread first = new Thread(task, "coacd-smoke-1");
        Thread second = new Thread(task, "coacd-smoke-2");
        first.start();
        second.start();
        start.countDown();
        try {
            first.join();
            second.join();
        } catch (InterruptedException error) {
            Thread.currentThread().interrupt();
            throw new AssertionError("concurrency test was interrupted", error);
        }
        if (failure.get() != null) {
            throw new AssertionError("concurrent decomposition failed", failure.get());
        }
    }

    private static void verifyInFlightInterruption() {
        CoacdNative.Parameters parameters = CoacdNative.Parameters.builder()
                .threshold(0.0)
                .preprocessMode(CoacdNative.PreprocessMode.OFF)
                .sampleResolution(100)
                .mctsIterations(100_000)
                .merge(false)
                .build();
        AtomicReference<Throwable> failure = new AtomicReference<>();
        CountDownLatch entered = new CountDownLatch(1);
        Thread worker = new Thread(() -> {
            entered.countDown();
            try {
                CoacdNative.decompose(twoTetrahedra(), parameters);
                failure.set(new AssertionError("interrupted decomposition completed normally"));
            } catch (CancellationException expected) {
                if (!Thread.currentThread().isInterrupted()) {
                    failure.set(new AssertionError("in-flight cancellation cleared interrupt status"));
                }
            } catch (Throwable error) {
                failure.set(error);
            }
        }, "coacd-cancellation-smoke");
        worker.setDaemon(true);
        worker.start();
        try {
            entered.await();
            Thread.sleep(25);
            worker.interrupt();
            worker.join(5_000);
        } catch (InterruptedException error) {
            Thread.currentThread().interrupt();
            throw new AssertionError("cancellation test was interrupted", error);
        }
        if (worker.isAlive()) {
            throw new AssertionError("native decomposition did not respond to interruption");
        }
        if (failure.get() != null) {
            throw new AssertionError("in-flight cancellation failed", failure.get());
        }
    }

    private static CoacdNative.Mesh tetrahedron() {
        return new CoacdNative.Mesh(
                new double[] {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1},
                new int[] {0, 2, 1, 0, 1, 3, 1, 2, 3, 2, 0, 3});
    }

    private static CoacdNative.Mesh twoTetrahedra() {
        return new CoacdNative.Mesh(
                new double[] {
                        0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1,
                        3, 0, 0, 4, 0, 0, 3, 1, 0, 3, 0, 1
                },
                new int[] {
                        0, 2, 1, 0, 1, 3, 1, 2, 3, 2, 0, 3,
                        4, 6, 5, 4, 5, 7, 5, 6, 7, 6, 4, 7
                });
    }

    private static void verifyIsolatedClassLoaderLoad() {
        URL classes = CoacdNative.class.getProtectionDomain().getCodeSource().getLocation();
        for (int i = 0; i < 2; ++i) {
            try (URLClassLoader loader = new URLClassLoader(
                    new URL[] {classes}, ClassLoader.getPlatformClassLoader())) {
                Class<?> binding = Class.forName(
                        "io.github.nivalis9.coacd.CoacdNative", true, loader);
                binding.getMethod("setLogLevel", String.class).invoke(null, "off");
            } catch (ReflectiveOperationException | java.io.IOException error) {
                throw new AssertionError("isolated class-loader native load failed", error);
            }
        }
    }

    private static void verifyNativeExceptionRecovery() {
        CoacdNative.Mesh openTriangle = new CoacdNative.Mesh(
                new double[] {0, 0, 0, 1, 0, 0, 0, 1, 0},
                new int[] {0, 1, 2});
        CoacdNative.Parameters parameters = CoacdNative.Parameters.builder()
                .preprocessMode(CoacdNative.PreprocessMode.OFF)
                .build();
        expectException(RuntimeException.class,
                () -> CoacdNative.decompose(openTriangle, parameters));
    }

    private static void verifyNativeLoadRetry() {
        String configuredPath = System.getProperty("coacd.library.path");
        if (configuredPath == null || configuredPath.trim().isEmpty()) return;
        System.setProperty("coacd.library.path", configuredPath + ".missing");
        try {
            expectException(UnsatisfiedLinkError.class,
                    () -> CoacdNative.setLogLevel("off"));
        } finally {
            System.setProperty("coacd.library.path", configuredPath);
        }
    }

    private static void expectException(Class<? extends Throwable> type, Runnable action) {
        try {
            action.run();
        } catch (Throwable error) {
            if (type.isInstance(error)) return;
            throw new AssertionError("expected " + type.getName() + " but got " + error, error);
        }
        throw new AssertionError("expected " + type.getName());
    }
}
