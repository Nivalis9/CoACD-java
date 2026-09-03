#include "io_github_nivalis9_coacd_CoacdNative_NativeBindings.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <limits>
#include <mutex>
#include <new>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "coacd.h"

namespace {
constexpr jsize kTransferTriples = 1024;
constexpr jsize kTransferValues = kTransferTriples * 3;

// A single CoACD invocation already parallelizes internally. Serializing JNI
// entry prevents N Java callers from each creating a full native worker pool
// and holding several large decomposition workspaces at the same time.
std::mutex decomposition_mutex;

class JavaCancellation final : public std::exception {
 public:
  const char *what() const noexcept override { return "CoACD operation cancelled"; }
};

void throw_java(JNIEnv *env, const char *class_name, const char *message) {
  if (env->ExceptionCheck()) return;
  jclass exception_class = env->FindClass(class_name);
  if (exception_class != nullptr) {
    env->ThrowNew(exception_class, message);
    env->DeleteLocalRef(exception_class);
  }
}

class ThreadInterruptionMonitor {
 public:
  explicit ThreadInterruptionMonitor(JNIEnv *env) : env_(env) {
    thread_class_ = env_->FindClass("java/lang/Thread");
    if (thread_class_ == nullptr) return;
    jmethodID current_thread =
        env_->GetStaticMethodID(thread_class_, "currentThread", "()Ljava/lang/Thread;");
    is_interrupted_ = env_->GetMethodID(thread_class_, "isInterrupted", "()Z");
    if (current_thread == nullptr || is_interrupted_ == nullptr) return;
    thread_ = env_->CallStaticObjectMethod(thread_class_, current_thread);
  }

  ThreadInterruptionMonitor(const ThreadInterruptionMonitor &) = delete;
  ThreadInterruptionMonitor &operator=(const ThreadInterruptionMonitor &) = delete;

  ~ThreadInterruptionMonitor() {
    if (thread_ != nullptr) env_->DeleteLocalRef(thread_);
    if (thread_class_ != nullptr) env_->DeleteLocalRef(thread_class_);
  }

  bool valid() const {
    return thread_class_ != nullptr && thread_ != nullptr &&
           is_interrupted_ != nullptr && !env_->ExceptionCheck();
  }

  bool interrupted() const {
    const jboolean value = env_->CallBooleanMethod(thread_, is_interrupted_);
    return env_->ExceptionCheck() || value == JNI_TRUE;
  }

 private:
  JNIEnv *env_;
  jclass thread_class_ = nullptr;
  jobject thread_ = nullptr;
  jmethodID is_interrupted_ = nullptr;
};

bool copy_vertices_from_java(JNIEnv *env, jdoubleArray source, jsize value_count,
                             std::vector<std::array<double, 3>> &destination,
                             const ThreadInterruptionMonitor &interruption) {
  destination.resize(static_cast<std::size_t>(value_count / 3));
  std::array<jdouble, kTransferValues> buffer{};
  for (jsize offset = 0; offset < value_count; offset += kTransferValues) {
    if (interruption.interrupted()) throw JavaCancellation();
    const jsize count = std::min(kTransferValues, value_count - offset);
    env->GetDoubleArrayRegion(source, offset, count, buffer.data());
    if (env->ExceptionCheck()) return false;
    for (jsize i = 0; i < count; i += 3) {
      destination[static_cast<std::size_t>((offset + i) / 3)] =
          {buffer[static_cast<std::size_t>(i)],
           buffer[static_cast<std::size_t>(i + 1)],
           buffer[static_cast<std::size_t>(i + 2)]};
    }
  }
  return true;
}

bool copy_triangles_from_java(JNIEnv *env, jintArray source, jsize value_count,
                              std::vector<std::array<int, 3>> &destination,
                              const ThreadInterruptionMonitor &interruption) {
  destination.resize(static_cast<std::size_t>(value_count / 3));
  std::array<jint, kTransferValues> buffer{};
  for (jsize offset = 0; offset < value_count; offset += kTransferValues) {
    if (interruption.interrupted()) throw JavaCancellation();
    const jsize count = std::min(kTransferValues, value_count - offset);
    env->GetIntArrayRegion(source, offset, count, buffer.data());
    if (env->ExceptionCheck()) return false;
    for (jsize i = 0; i < count; i += 3) {
      destination[static_cast<std::size_t>((offset + i) / 3)] =
          {static_cast<int>(buffer[static_cast<std::size_t>(i)]),
           static_cast<int>(buffer[static_cast<std::size_t>(i + 1)]),
           static_cast<int>(buffer[static_cast<std::size_t>(i + 2)])};
    }
  }
  return true;
}

bool copy_vertices_to_java(JNIEnv *env,
                           const std::vector<std::array<double, 3>> &source,
                           jdoubleArray destination,
                           const ThreadInterruptionMonitor &interruption) {
  std::array<jdouble, kTransferValues> buffer{};
  for (std::size_t offset = 0; offset < source.size(); offset += kTransferTriples) {
    if (interruption.interrupted()) throw JavaCancellation();
    const std::size_t count =
        std::min<std::size_t>(kTransferTriples, source.size() - offset);
    for (std::size_t i = 0; i < count; ++i) {
      buffer[3 * i] = source[offset + i][0];
      buffer[3 * i + 1] = source[offset + i][1];
      buffer[3 * i + 2] = source[offset + i][2];
    }
    env->SetDoubleArrayRegion(destination, static_cast<jsize>(offset * 3),
                              static_cast<jsize>(count * 3), buffer.data());
    if (env->ExceptionCheck()) return false;
  }
  return true;
}

bool copy_triangles_to_java(JNIEnv *env,
                            const std::vector<std::array<int, 3>> &source,
                            jintArray destination,
                            const ThreadInterruptionMonitor &interruption) {
  std::array<jint, kTransferValues> buffer{};
  for (std::size_t offset = 0; offset < source.size(); offset += kTransferTriples) {
    if (interruption.interrupted()) throw JavaCancellation();
    const std::size_t count =
        std::min<std::size_t>(kTransferTriples, source.size() - offset);
    for (std::size_t i = 0; i < count; ++i) {
      buffer[3 * i] = static_cast<jint>(source[offset + i][0]);
      buffer[3 * i + 1] = static_cast<jint>(source[offset + i][1]);
      buffer[3 * i + 2] = static_cast<jint>(source[offset + i][2]);
    }
    env->SetIntArrayRegion(destination, static_cast<jsize>(offset * 3),
                           static_cast<jsize>(count * 3), buffer.data());
    if (env->ExceptionCheck()) return false;
  }
  return true;
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
      throw_java(env, "java/lang/IllegalArgumentException",
                 "mesh arrays must contain xyz/ijk triples");
      return nullptr;
    }
    if (preprocess_mode < 0 || preprocess_mode > 2 ||
        approximation_mode < 0 || approximation_mode > 1 || seed < 0 ||
        static_cast<unsigned long long>(seed) >
            std::numeric_limits<unsigned int>::max()) {
      throw_java(env, "java/lang/IllegalArgumentException",
                 "invalid native CoACD option value");
      return nullptr;
    }

    ThreadInterruptionMonitor interruption(env);
    if (!interruption.valid()) return nullptr;

    coacd::Mesh input;
    if (!copy_vertices_from_java(env, vertices, vertex_value_count, input.vertices,
                                 interruption) ||
        !copy_triangles_from_java(env, triangles, triangle_value_count, input.indices,
                                  interruption))
      return nullptr;

    std::unique_lock<std::mutex> decomposition_lock(decomposition_mutex,
                                                    std::defer_lock);
    while (!decomposition_lock.try_lock()) {
      if (interruption.interrupted()) throw JavaCancellation();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (interruption.interrupted()) throw JavaCancellation();

    const char *preprocess =
        preprocess_mode == 0 ? "auto" : preprocess_mode == 1 ? "on" : "off";
    const char *approximation = approximation_mode == 0 ? "ch" : "box";

    std::atomic_bool cancellation_requested{false};
    std::vector<coacd::Mesh> result;
    std::exception_ptr worker_error;
    std::mutex completion_mutex;
    std::condition_variable completion_condition;
    bool complete = false;

    std::jthread worker([&] {
      try {
        result = coacd::CoACDWithCancellation(
            std::move(input), threshold, max_convex_hulls, preprocess, preprocess_resolution,
            sample_resolution, mcts_nodes, mcts_iterations, mcts_max_depth,
            pca == JNI_TRUE, merge == JNI_TRUE, decimate == JNI_TRUE,
            max_convex_hull_vertices, extrude == JNI_TRUE, extrude_margin,
            approximation, static_cast<unsigned int>(seed), real_metric == JNI_TRUE,
            &cancellation_requested);
      } catch (...) {
        worker_error = std::current_exception();
      }
      {
        std::lock_guard<std::mutex> lock(completion_mutex);
        complete = true;
      }
      completion_condition.notify_one();
    });

    {
      std::unique_lock<std::mutex> completion_lock(completion_mutex);
      while (!complete) {
        completion_condition.wait_for(completion_lock, std::chrono::milliseconds(20));
        if (interruption.interrupted())
          cancellation_requested.store(true, std::memory_order_relaxed);
      }
    }
    worker.join();
    if (env->ExceptionCheck()) return nullptr;
    if (cancellation_requested.load(std::memory_order_relaxed))
      throw JavaCancellation();
    if (worker_error) std::rethrow_exception(worker_error);

    // The worker no longer needs the input. Release it before allocating Java
    // output arrays so both full native buffers do not overlap in peak memory.
    std::vector<std::array<double, 3>>().swap(input.vertices);
    std::vector<std::array<int, 3>>().swap(input.indices);

    if (result.size() > static_cast<std::size_t>(INT32_MAX))
      throw std::runtime_error("CoACD returned too many meshes for a Java array");
    jclass mesh_class =
        env->FindClass("io/github/nivalis9/coacd/CoacdNative$Mesh");
    if (mesh_class == nullptr) return nullptr;
    jmethodID constructor = env->GetMethodID(mesh_class, "<init>", "([D[IZ)V");
    if (constructor == nullptr) {
      env->DeleteLocalRef(mesh_class);
      return nullptr;
    }
    jobjectArray output = env->NewObjectArray(
        static_cast<jsize>(result.size()), mesh_class, nullptr);
    if (output == nullptr) {
      env->DeleteLocalRef(mesh_class);
      return nullptr;
    }

    for (jsize i = 0; i < static_cast<jsize>(result.size()); ++i) {
      if (interruption.interrupted()) throw JavaCancellation();
      coacd::Mesh &part = result[static_cast<std::size_t>(i)];
      if (part.vertices.size() > static_cast<std::size_t>(INT32_MAX / 3) ||
          part.indices.size() > static_cast<std::size_t>(INT32_MAX / 3))
        throw std::runtime_error("CoACD returned a mesh too large for Java arrays");

      const jsize vertex_count = static_cast<jsize>(part.vertices.size() * 3);
      const jsize triangle_count = static_cast<jsize>(part.indices.size() * 3);
      jdoubleArray java_vertices = env->NewDoubleArray(vertex_count);
      if (java_vertices == nullptr) {
        env->DeleteLocalRef(mesh_class);
        return nullptr;
      }
      jintArray java_triangles = env->NewIntArray(triangle_count);
      if (java_triangles == nullptr) {
        env->DeleteLocalRef(java_vertices);
        env->DeleteLocalRef(mesh_class);
        return nullptr;
      }
      if (!copy_vertices_to_java(env, part.vertices, java_vertices, interruption) ||
          !copy_triangles_to_java(env, part.indices, java_triangles, interruption)) {
        env->DeleteLocalRef(java_vertices);
        env->DeleteLocalRef(java_triangles);
        env->DeleteLocalRef(mesh_class);
        return nullptr;
      }

      jobject java_mesh = env->NewObject(mesh_class, constructor, java_vertices,
                                         java_triangles, JNI_TRUE);
      if (!env->ExceptionCheck() && java_mesh != nullptr)
        env->SetObjectArrayElement(output, i, java_mesh);
      env->DeleteLocalRef(java_vertices);
      env->DeleteLocalRef(java_triangles);
      if (java_mesh != nullptr) env->DeleteLocalRef(java_mesh);
      if (env->ExceptionCheck()) {
        env->DeleteLocalRef(mesh_class);
        return nullptr;
      }

      // Java now owns the arrays, so release each native part immediately.
      std::vector<std::array<double, 3>>().swap(part.vertices);
      std::vector<std::array<int, 3>>().swap(part.indices);
    }
    env->DeleteLocalRef(mesh_class);
    return output;
  } catch (const JavaCancellation &error) {
    throw_java(env, "java/util/concurrent/CancellationException", error.what());
  } catch (const std::bad_alloc &) {
    throw_java(env, "java/lang/OutOfMemoryError", "native CoACD allocation failed");
  } catch (const std::invalid_argument &error) {
    throw_java(env, "java/lang/IllegalArgumentException", error.what());
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
