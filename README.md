# CoACD JNI bindings

Java bindings for the CoACD C++ API. Mesh coordinates and triangle indices use
flat arrays (`x, y, z, ...` and `i, j, k, ...`).

## Build

CoACD 1.0.14 and its pinned CDT headers are included under `vendor/CoACD`.
Requirements are JDK 11+, CMake 3.24+, and a C++20 compiler. From this
directory:

```powershell
cmake -S . -B build
cmake --build build --config Release --target coacd_jni
mvn package
```

An alternate CoACD checkout can still be selected with
`-DCOACD_SOURCE_DIR="C:\path\to\CoACD"`.

The checked-in JNI header can be regenerated after changing native Java method
signatures with `javac --release 11 -h native -d out` followed by the Java
source path.

CoACD builds third-party preprocessing support by default. To make a smaller
build that only accepts manifold input, add `-DWITH_3RD_PARTY_LIBS=OFF` to the
configure command.

JNI calls are safe from multiple Java threads, but decompositions are queued
within each loaded native library because a single CoACD operation already uses
native parallelism and a large working set. Interrupting a Java thread running
`decompose` requests cooperative cancellation and results in a
`java.util.concurrent.CancellationException`; the thread's interrupted status
is preserved. Cancellation can be delayed while an individual third-party
preprocessing operation is inside OpenVDB.

At runtime, put `coacd_jni` on `java.library.path`, or pass its full path:

```powershell
java --enable-native-access=ALL-UNNAMED -Dcoacd.library.path="C:\full\path\to\coacd_jni.dll" -cp target\coacd-java-1.0.14-SNAPSHOT.jar YourMainClass
```

Linux and macOS use the corresponding `libcoacd_jni.so` or `libcoacd_jni.dylib`.

## Usage

```java
import io.github.nivalis9.coacd.CoacdNative;

double[] vertices = {
    0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1
};
int[] triangles = {
    0, 2, 1, 0, 1, 3, 1, 2, 3, 2, 0, 3
};

CoacdNative.Mesh input = new CoacdNative.Mesh(vertices, triangles);
CoacdNative.Parameters parameters = CoacdNative.Parameters.builder()
    .threshold(0.05)
    .maxConvexHulls(16)
    .preprocessMode(CoacdNative.PreprocessMode.AUTO)
    .build();

CoacdNative.Mesh[] convexParts = CoacdNative.decompose(input, parameters);
```

To provide every option explicitly instead of starting from defaults:

```java
CoacdNative.Parameters parameters = new CoacdNative.Parameters(
    0.05,  // threshold
    -1,    // maximum convex hulls (-1 means unlimited)
    CoacdNative.PreprocessMode.AUTO,
    50,    // preprocess resolution
    2000,  // sample resolution
    20,    // MCTS nodes
    150,   // MCTS iterations
    3,     // MCTS maximum depth
    false, // PCA
    true,  // merge
    false, // decimate
    256,   // maximum vertices per convex hull
    false, // extrude
    0.01,  // extrude margin
    CoacdNative.ApproximationMode.CONVEX_HULL,
    0,     // unsigned 32-bit seed
    false  // real-metric threshold
);
```

The standalone native smoke test can be run without a test framework:

```powershell
javac --release 11 -d out src\main\java\io\github\nivalis9\coacd\CoacdNative.java src\test\java\io\github\nivalis9\coacd\CoacdNativeSmokeTest.java
java --enable-native-access=ALL-UNNAMED -Dcoacd.library.path="build\Release\coacd_jni.dll" -cp out io.github.nivalis9.coacd.CoacdNativeSmokeTest
```

The `--enable-native-access` option suppresses the native-access warning on
recent JDKs. It is not needed on JDK 11.
