package io.github.nivalis9.coacd;

import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Objects;

/** Java bindings for CoACD (Approximate Convex Decomposition). */
public final class CoacdNative {
    private CoacdNative() {}

    /** A triangular mesh. Vertices and triangles are stored as flat xyz/ijk triples. */
    public static final class Mesh {
        private final double[] vertices;
        private final int[] triangles;

        public Mesh(double[] vertices, int[] triangles) {
            this.vertices = Objects.requireNonNull(vertices, "vertices").clone();
            this.triangles = Objects.requireNonNull(triangles, "triangles").clone();
            validateMesh(this.vertices, this.triangles);
        }

        public double[] getVertices() { return vertices.clone(); }
        public int[] getTriangles() { return triangles.clone(); }
        public int getVertexCount() { return vertices.length / 3; }
        public int getTriangleCount() { return triangles.length / 3; }
    }

    public enum PreprocessMode {
        AUTO(0), ON(1), OFF(2);
        private final int nativeValue;
        PreprocessMode(int nativeValue) { this.nativeValue = nativeValue; }
    }

    public enum ApproximationMode {
        CONVEX_HULL(0), BOX(1);
        private final int nativeValue;
        ApproximationMode(int nativeValue) { this.nativeValue = nativeValue; }
    }

    /**
     * CoACD parameters. Pass every value to the public constructor, or use
     * {@link #builder()} when starting from the upstream defaults is convenient.
     */
    public static final class Parameters {
        private final double threshold;
        private final int maxConvexHulls;
        private final PreprocessMode preprocessMode;
        private final int preprocessResolution;
        private final int sampleResolution;
        private final int mctsNodes;
        private final int mctsIterations;
        private final int mctsMaxDepth;
        private final boolean pca;
        private final boolean merge;
        private final boolean decimate;
        private final int maxConvexHullVertices;
        private final boolean extrude;
        private final double extrudeMargin;
        private final ApproximationMode approximationMode;
        private final long seed;
        private final boolean realMetric;

        /** Creates a configuration from values supplied entirely by the caller. */
        public Parameters(
                double threshold,
                int maxConvexHulls,
                PreprocessMode preprocessMode,
                int preprocessResolution,
                int sampleResolution,
                int mctsNodes,
                int mctsIterations,
                int mctsMaxDepth,
                boolean pca,
                boolean merge,
                boolean decimate,
                int maxConvexHullVertices,
                boolean extrude,
                double extrudeMargin,
                ApproximationMode approximationMode,
                long seed,
                boolean realMetric) {
            this.threshold = threshold;
            this.maxConvexHulls = maxConvexHulls;
            this.preprocessMode = preprocessMode;
            this.preprocessResolution = preprocessResolution;
            this.sampleResolution = sampleResolution;
            this.mctsNodes = mctsNodes;
            this.mctsIterations = mctsIterations;
            this.mctsMaxDepth = mctsMaxDepth;
            this.pca = pca;
            this.merge = merge;
            this.decimate = decimate;
            this.maxConvexHullVertices = maxConvexHullVertices;
            this.extrude = extrude;
            this.extrudeMargin = extrudeMargin;
            this.approximationMode = approximationMode;
            this.seed = seed;
            this.realMetric = realMetric;
            validateParameters(this);
        }

        private Parameters(Builder b) {
            this(b.threshold, b.maxConvexHulls, b.preprocessMode,
                    b.preprocessResolution, b.sampleResolution, b.mctsNodes,
                    b.mctsIterations, b.mctsMaxDepth, b.pca, b.merge,
                    b.decimate, b.maxConvexHullVertices, b.extrude,
                    b.extrudeMargin, b.approximationMode, b.seed, b.realMetric);
        }

        public double getThreshold() { return threshold; }
        public int getMaxConvexHulls() { return maxConvexHulls; }
        public PreprocessMode getPreprocessMode() { return preprocessMode; }
        public int getPreprocessResolution() { return preprocessResolution; }
        public int getSampleResolution() { return sampleResolution; }
        public int getMctsNodes() { return mctsNodes; }
        public int getMctsIterations() { return mctsIterations; }
        public int getMctsMaxDepth() { return mctsMaxDepth; }
        public boolean isPca() { return pca; }
        public boolean isMerge() { return merge; }
        public boolean isDecimate() { return decimate; }
        public int getMaxConvexHullVertices() { return maxConvexHullVertices; }
        public boolean isExtrude() { return extrude; }
        public double getExtrudeMargin() { return extrudeMargin; }
        public ApproximationMode getApproximationMode() { return approximationMode; }
        public long getSeed() { return seed; }
        public boolean isRealMetric() { return realMetric; }

        public static Builder builder() { return new Builder(); }
        public static Parameters defaults() { return builder().build(); }

        public static final class Builder {
            private double threshold = 0.05;
            private int maxConvexHulls = -1;
            private PreprocessMode preprocessMode = PreprocessMode.AUTO;
            private int preprocessResolution = 50;
            private int sampleResolution = 2000;
            private int mctsNodes = 20;
            private int mctsIterations = 150;
            private int mctsMaxDepth = 3;
            private boolean pca;
            private boolean merge = true;
            private boolean decimate;
            private int maxConvexHullVertices = 256;
            private boolean extrude;
            private double extrudeMargin = 0.01;
            private ApproximationMode approximationMode = ApproximationMode.CONVEX_HULL;
            private long seed;
            private boolean realMetric;

            public Builder threshold(double value) { threshold = value; return this; }
            public Builder maxConvexHulls(int value) { maxConvexHulls = value; return this; }
            public Builder preprocessMode(PreprocessMode value) { preprocessMode = value; return this; }
            public Builder preprocessResolution(int value) { preprocessResolution = value; return this; }
            public Builder sampleResolution(int value) { sampleResolution = value; return this; }
            public Builder mctsNodes(int value) { mctsNodes = value; return this; }
            public Builder mctsIterations(int value) { mctsIterations = value; return this; }
            public Builder mctsMaxDepth(int value) { mctsMaxDepth = value; return this; }
            public Builder pca(boolean value) { pca = value; return this; }
            public Builder merge(boolean value) { merge = value; return this; }
            public Builder decimate(boolean value) { decimate = value; return this; }
            public Builder maxConvexHullVertices(int value) { maxConvexHullVertices = value; return this; }
            public Builder extrude(boolean value) { extrude = value; return this; }
            public Builder extrudeMargin(double value) { extrudeMargin = value; return this; }
            public Builder approximationMode(ApproximationMode value) { approximationMode = value; return this; }
            public Builder seed(long value) { seed = value; return this; }
            public Builder realMetric(boolean value) { realMetric = value; return this; }
            public Parameters build() { return new Parameters(this); }
        }
    }

    public static Mesh[] decompose(Mesh input) {
        return decompose(input, Parameters.defaults());
    }

    public static Mesh[] decompose(Mesh input, Parameters parameters) {
        Objects.requireNonNull(input, "input");
        Objects.requireNonNull(parameters, "parameters");
        return NativeBindings.decompose(
                input.vertices, input.triangles, parameters.threshold,
                parameters.maxConvexHulls, parameters.preprocessMode.nativeValue,
                parameters.preprocessResolution, parameters.sampleResolution,
                parameters.mctsNodes, parameters.mctsIterations, parameters.mctsMaxDepth,
                parameters.pca, parameters.merge, parameters.decimate,
                parameters.maxConvexHullVertices, parameters.extrude,
                parameters.extrudeMargin, parameters.approximationMode.nativeValue,
                parameters.seed, parameters.realMetric);
    }

    /** Sets CoACD's native log level: off, debug, info, warn, error, or critical. */
    public static void setLogLevel(String level) {
        Objects.requireNonNull(level, "level");
        switch (level) {
            case "off":
            case "debug":
            case "info":
            case "warn":
            case "warning":
            case "error":
            case "err":
            case "critical":
                break;
            default:
                throw new IllegalArgumentException("invalid CoACD log level: " + level);
        }
        NativeBindings.setLogLevel(level);
    }

    private static void validateMesh(double[] vertices, int[] triangles) {
        if (vertices.length == 0 || vertices.length % 3 != 0) {
            throw new IllegalArgumentException("vertices must contain non-empty xyz triples");
        }
        if (triangles.length == 0 || triangles.length % 3 != 0) {
            throw new IllegalArgumentException("triangles must contain non-empty index triples");
        }
        for (double coordinate : vertices) {
            if (!Double.isFinite(coordinate)) {
                throw new IllegalArgumentException("vertex coordinates must be finite");
            }
        }
        int vertexCount = vertices.length / 3;
        if (vertexCount < 3) {
            throw new IllegalArgumentException("a mesh must contain at least three vertices");
        }
        double minX = vertices[0];
        double maxX = vertices[0];
        double minY = vertices[1];
        double maxY = vertices[1];
        double minZ = vertices[2];
        double maxZ = vertices[2];
        for (int i = 3; i < vertices.length; i += 3) {
            minX = Math.min(minX, vertices[i]);
            maxX = Math.max(maxX, vertices[i]);
            minY = Math.min(minY, vertices[i + 1]);
            maxY = Math.max(maxY, vertices[i + 1]);
            minZ = Math.min(minZ, vertices[i + 2]);
            maxZ = Math.max(maxZ, vertices[i + 2]);
        }
        double extent = Math.max(Math.max(maxX - minX, maxY - minY), maxZ - minZ);
        if (!Double.isFinite(extent) || extent <= 0.0
                || !Double.isFinite(maxX + minX)
                || !Double.isFinite(maxY + minY)
                || !Double.isFinite(maxZ + minZ)) {
            throw new IllegalArgumentException("mesh bounding box cannot be normalized safely");
        }
        for (int index : triangles) {
            if (index < 0 || index >= vertexCount) {
                throw new IllegalArgumentException("triangle index out of range: " + index);
            }
        }
    }

    private static void validateParameters(Parameters p) {
        if (!Double.isFinite(p.threshold) || p.threshold < 0.0 || (!p.realMetric && p.threshold > 1.0)) {
            throw new IllegalArgumentException("threshold must be finite and >= 0 (and <= 1 unless realMetric is enabled)");
        }
        if (p.maxConvexHulls == 0 || p.maxConvexHulls < -1) {
            throw new IllegalArgumentException("maxConvexHulls must be -1 or positive");
        }
        Objects.requireNonNull(p.preprocessMode, "preprocessMode");
        Objects.requireNonNull(p.approximationMode, "approximationMode");
        if (p.preprocessResolution < 5 || p.preprocessResolution > 1000) {
            throw new IllegalArgumentException("preprocessResolution must be between 5 and 1000");
        }
        if (p.sampleResolution <= 0 || p.mctsNodes <= 0 || p.mctsIterations <= 0
                || p.mctsMaxDepth <= 0 || p.maxConvexHullVertices <= 0) {
            throw new IllegalArgumentException("resolution, MCTS, and hull vertex limits must be positive");
        }
        if (!Double.isFinite(p.extrudeMargin) || p.extrudeMargin < 0.0) {
            throw new IllegalArgumentException("extrudeMargin must be finite and >= 0");
        }
        if (p.seed < 0 || p.seed > 0xffff_ffffL) {
            throw new IllegalArgumentException("seed must fit an unsigned 32-bit integer");
        }
    }

    private static final class NativeBindings {
        static {
            String explicitPath = System.getProperty("coacd.library.path");
            if (explicitPath == null || explicitPath.trim().isEmpty()) {
                System.loadLibrary("coacd_jni");
            } else {
                Path path = Paths.get(explicitPath).toAbsolutePath().normalize();
                System.load(path.toString());
            }
        }

        private static native Mesh[] decompose(
                double[] vertices, int[] triangles, double threshold,
                int maxConvexHulls, int preprocessMode, int preprocessResolution,
                int sampleResolution, int mctsNodes, int mctsIterations,
                int mctsMaxDepth, boolean pca, boolean merge, boolean decimate,
                int maxConvexHullVertices, boolean extrude, double extrudeMargin,
                int approximationMode, long seed, boolean realMetric);

        private static native void setLogLevel(String level);
    }
}
