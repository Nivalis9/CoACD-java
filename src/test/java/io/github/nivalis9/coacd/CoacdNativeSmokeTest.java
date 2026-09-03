package io.github.nivalis9.coacd;

/** Minimal native smoke test; run with assertions enabled. */
public final class CoacdNativeSmokeTest {
    private CoacdNativeSmokeTest() {}

    public static void main(String[] args) {
        verifyJavaValidation();
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
                () -> CoacdNative.Parameters.builder().seed(0x1_0000_0000L).build());
        expectException(IllegalArgumentException.class,
                () -> CoacdNative.setLogLevel("verbose"));
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
