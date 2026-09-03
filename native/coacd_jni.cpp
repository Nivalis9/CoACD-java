#include "io_github_nivalis9_coacd_CoacdNative_NativeBindings.h"

#include <array>
#include <cstdint>
#include <exception>
#include <new>
#include <limits>
#include <stdexcept>
#include <vector>

#include "coacd.h"

namespace {
void throw_java(JNIEnv *env, const char *class_name, const char *message) {
  if (env->ExceptionCheck()) return;
  jclass exception_class = env->FindClass(class_name);
  if (exception_class != nullptr) {
    env->ThrowNew(exception_class, message);
    env->DeleteLocalRef(exception_class);
  }
}
} // namespace

extern "C" JNIEXPORT jobjectArray JNICALL
Java_io_github_nivalis9_coacd_CoacdNative_00024NativeBindings_decompose(
    JNIEnv *env, jclass, jdoubleArray vertices, jintArray triangles,
    jdouble threshold, jint max_convex_hulls, jint preprocess_mode,
    jint preprocess_resolution, jint sample_resolution, jint mcts_nodes,
    jint mcts_iterations, jint mcts_max_depth, jboolean pca, jboolean merge,
    jboolean decimate, jint max_convex_hull_vertices, jboolean extrude,
    jdouble extrude_margin, jint approximation_mode, jlong seed,
    jboolean real_metric) {
  try {
    if (vertices == nullptr || triangles == nullptr) {
      throw_java(env, "java/lang/NullPointerException", "mesh arrays must not be null");
      return nullptr;
    }
    const jsize vertex_value_count = env->GetArrayLength(vertices);
    const jsize triangle_value_count = env->GetArrayLength(triangles);
    if (vertex_value_count == 0 || vertex_value_count % 3 != 0 ||
        triangle_value_count == 0 || triangle_value_count % 3 != 0) {
      throw_java(env, "java/lang/IllegalArgumentException", "mesh arrays must contain xyz/ijk triples");
      return nullptr;
    }
    if (preprocess_mode < 0 || preprocess_mode > 2 ||
        approximation_mode < 0 || approximation_mode > 1 || seed < 0 ||
        static_cast<unsigned long long>(seed) > std::numeric_limits<unsigned int>::max()) {
      throw_java(env, "java/lang/IllegalArgumentException", "invalid native CoACD option value");
      return nullptr;
    }
    std::vector<jdouble> vertex_values(static_cast<std::size_t>(vertex_value_count));
    std::vector<jint> triangle_values(static_cast<std::size_t>(triangle_value_count));
    env->GetDoubleArrayRegion(vertices, 0, vertex_value_count, vertex_values.data());
    if (env->ExceptionCheck()) return nullptr;
    env->GetIntArrayRegion(triangles, 0, triangle_value_count, triangle_values.data());
    if (env->ExceptionCheck()) return nullptr;

    coacd::Mesh input;
    input.vertices.reserve(static_cast<std::size_t>(vertex_value_count / 3));
    input.indices.reserve(static_cast<std::size_t>(triangle_value_count / 3));
    for (jsize i = 0; i < vertex_value_count; i += 3)
      input.vertices.push_back({vertex_values[i], vertex_values[i + 1], vertex_values[i + 2]});
    for (jsize i = 0; i < triangle_value_count; i += 3)
      input.indices.push_back({triangle_values[i], triangle_values[i + 1], triangle_values[i + 2]});

    const char *preprocess = preprocess_mode == 0 ? "auto" : preprocess_mode == 1 ? "on" : "off";
    const char *approximation = approximation_mode == 0 ? "ch" : "box";
    std::vector<coacd::Mesh> result = coacd::CoACD(
        input, threshold, max_convex_hulls, preprocess, preprocess_resolution,
        sample_resolution, mcts_nodes, mcts_iterations, mcts_max_depth,
        pca == JNI_TRUE, merge == JNI_TRUE, decimate == JNI_TRUE,
        max_convex_hull_vertices, extrude == JNI_TRUE, extrude_margin,
        approximation, static_cast<unsigned int>(seed), real_metric == JNI_TRUE);

    if (result.size() > static_cast<std::size_t>(INT32_MAX))
      throw std::runtime_error("CoACD returned too many meshes for a Java array");
    jclass mesh_class = env->FindClass("io/github/nivalis9/coacd/CoacdNative$Mesh");
    if (mesh_class == nullptr) return nullptr;
    jmethodID constructor = env->GetMethodID(mesh_class, "<init>", "([D[I)V");
    if (constructor == nullptr) { env->DeleteLocalRef(mesh_class); return nullptr; }
    jobjectArray output = env->NewObjectArray(static_cast<jsize>(result.size()), mesh_class, nullptr);
    if (output == nullptr) { env->DeleteLocalRef(mesh_class); return nullptr; }

    for (jsize i = 0; i < static_cast<jsize>(result.size()); ++i) {
      const coacd::Mesh &part = result[static_cast<std::size_t>(i)];
      if (part.vertices.size() > static_cast<std::size_t>(INT32_MAX / 3) ||
          part.indices.size() > static_cast<std::size_t>(INT32_MAX / 3))
        throw std::runtime_error("CoACD returned a mesh too large for Java arrays");

      std::vector<jdouble> out_vertices;
      out_vertices.reserve(part.vertices.size() * 3);
      for (const std::array<double, 3> &vertex : part.vertices)
        out_vertices.insert(out_vertices.end(), vertex.begin(), vertex.end());
      std::vector<jint> out_triangles;
      out_triangles.reserve(part.indices.size() * 3);
      for (const std::array<int, 3> &triangle : part.indices)
        out_triangles.insert(out_triangles.end(), triangle.begin(), triangle.end());

      jdoubleArray java_vertices = env->NewDoubleArray(static_cast<jsize>(out_vertices.size()));
      if (java_vertices == nullptr) {
        env->DeleteLocalRef(mesh_class);
        return nullptr;
      }
      jintArray java_triangles = env->NewIntArray(static_cast<jsize>(out_triangles.size()));
      if (java_triangles == nullptr) {
        env->DeleteLocalRef(java_vertices);
        env->DeleteLocalRef(mesh_class);
        return nullptr;
      }
      env->SetDoubleArrayRegion(java_vertices, 0, static_cast<jsize>(out_vertices.size()), out_vertices.data());
      if (env->ExceptionCheck()) {
        env->DeleteLocalRef(java_vertices);
        env->DeleteLocalRef(java_triangles);
        env->DeleteLocalRef(mesh_class);
        return nullptr;
      }
      env->SetIntArrayRegion(java_triangles, 0, static_cast<jsize>(out_triangles.size()), out_triangles.data());
      if (env->ExceptionCheck()) {
        env->DeleteLocalRef(java_vertices);
        env->DeleteLocalRef(java_triangles);
        env->DeleteLocalRef(mesh_class);
        return nullptr;
      }
      jobject java_mesh = env->NewObject(mesh_class, constructor, java_vertices, java_triangles);
      if (!env->ExceptionCheck() && java_mesh != nullptr) env->SetObjectArrayElement(output, i, java_mesh);
      env->DeleteLocalRef(java_vertices);
      env->DeleteLocalRef(java_triangles);
      if (java_mesh != nullptr) env->DeleteLocalRef(java_mesh);
      if (env->ExceptionCheck()) { env->DeleteLocalRef(mesh_class); return nullptr; }
    }
    env->DeleteLocalRef(mesh_class);
    return output;
  } catch (const std::bad_alloc &) {
    throw_java(env, "java/lang/OutOfMemoryError", "native CoACD allocation failed");
  } catch (const std::exception &error) {
    throw_java(env, "java/lang/RuntimeException", error.what());
  } catch (...) {
    throw_java(env, "java/lang/RuntimeException", "unknown native CoACD failure");
  }
  return nullptr;
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_nivalis9_coacd_CoacdNative_00024NativeBindings_setLogLevel(
    JNIEnv *env, jclass, jstring level) {
  if (level == nullptr) {
    throw_java(env, "java/lang/NullPointerException", "level must not be null");
    return;
  }
  const char *utf = nullptr;
  try {
    utf = env->GetStringUTFChars(level, nullptr);
    if (utf == nullptr) return;
    coacd::set_log_level(utf);
    env->ReleaseStringUTFChars(level, utf);
  } catch (const std::bad_alloc &) {
    if (utf != nullptr) env->ReleaseStringUTFChars(level, utf);
    throw_java(env, "java/lang/OutOfMemoryError", "native CoACD allocation failed");
  } catch (const std::exception &error) {
    if (utf != nullptr) env->ReleaseStringUTFChars(level, utf);
    throw_java(env, "java/lang/IllegalArgumentException", error.what());
  } catch (...) {
    if (utf != nullptr) env->ReleaseStringUTFChars(level, utf);
    throw_java(env, "java/lang/RuntimeException", "unknown native CoACD failure");
  }
}
