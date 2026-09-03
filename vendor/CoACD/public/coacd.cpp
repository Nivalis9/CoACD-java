#include "coacd.h"
#include "../src/logger.h"
#if WITH_3RD_PARTY_LIBS
#include "../src/preprocess.h"
#endif
#include "../src/process.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {
void ValidateMesh(const coacd::Mesh &mesh) {
  if (mesh.vertices.size() < 3 || mesh.indices.empty())
    throw std::invalid_argument("CoACD mesh must contain vertices and triangles");

  double min_x = mesh.vertices[0][0], max_x = min_x;
  double min_y = mesh.vertices[0][1], max_y = min_y;
  double min_z = mesh.vertices[0][2], max_z = min_z;
  for (const auto &vertex : mesh.vertices) {
    for (double coordinate : vertex)
      if (!std::isfinite(coordinate))
        throw std::invalid_argument("CoACD vertex coordinates must be finite");
    min_x = std::min(min_x, vertex[0]);
    max_x = std::max(max_x, vertex[0]);
    min_y = std::min(min_y, vertex[1]);
    max_y = std::max(max_y, vertex[1]);
    min_z = std::min(min_z, vertex[2]);
    max_z = std::max(max_z, vertex[2]);
  }
  const double extent = std::max({max_x - min_x, max_y - min_y, max_z - min_z});
  if (!std::isfinite(extent) || extent <= 0.0 ||
      !std::isfinite(max_x + min_x) || !std::isfinite(max_y + min_y) ||
      !std::isfinite(max_z + min_z))
    throw std::invalid_argument("CoACD mesh bounding box cannot be normalized safely");

  for (std::size_t triangle_index = 0; triangle_index < mesh.indices.size(); ++triangle_index) {
    const auto &triangle = mesh.indices[triangle_index];
    const int i0 = triangle[0], i1 = triangle[1], i2 = triangle[2];
    if (i0 < 0 || i1 < 0 || i2 < 0 ||
        static_cast<std::size_t>(i0) >= mesh.vertices.size() ||
        static_cast<std::size_t>(i1) >= mesh.vertices.size() ||
        static_cast<std::size_t>(i2) >= mesh.vertices.size())
      throw std::invalid_argument("CoACD triangle index is out of range");
    if (i0 == i1 || i1 == i2 || i2 == i0)
      throw std::invalid_argument("CoACD triangle repeats a vertex index");

    const auto &a = mesh.vertices[static_cast<std::size_t>(i0)];
    const auto &b = mesh.vertices[static_cast<std::size_t>(i1)];
    const auto &c = mesh.vertices[static_cast<std::size_t>(i2)];
    const std::array<double, 3> ab{(b[0] - a[0]) / extent,
                                   (b[1] - a[1]) / extent,
                                   (b[2] - a[2]) / extent};
    const std::array<double, 3> ac{(c[0] - a[0]) / extent,
                                   (c[1] - a[1]) / extent,
                                   (c[2] - a[2]) / extent};
    const double cross_x = std::fma(ab[1], ac[2], -ab[2] * ac[1]);
    const double cross_y = std::fma(ab[2], ac[0], -ab[0] * ac[2]);
    const double cross_z = std::fma(ab[0], ac[1], -ab[1] * ac[0]);
    const double area_squared = std::fma(
        cross_x, cross_x, std::fma(cross_y, cross_y, cross_z * cross_z));
    if (!std::isfinite(area_squared) || area_squared <= 1.0e-30)
      throw std::invalid_argument("CoACD triangle has zero or unstable area");
  }
}
} // namespace

namespace coacd {
void RecoverParts(vector<Model> &meshes, vector<double> bbox,
                  array<array<double, 3>, 3> rot) {
  for (int i = 0; i < (int)meshes.size(); i++) {
    meshes[i].RevertPCA(rot);
    meshes[i].Recover(bbox);
  }
}

std::vector<Mesh> CoACDWithCancellation(Mesh input, double threshold,
                        int max_convex_hull, std::string preprocess_mode,
                        int prep_resolution, int sample_resolution,
                        int mcts_nodes, int mcts_iteration, int mcts_max_depth,
                        bool pca, bool merge, bool decimate, int max_ch_vertex,
                        bool extrude, double extrude_margin,
                        std::string apx_mode, unsigned int seed,
                        bool real_metric,
                        std::atomic_bool const *cancellation_requested) {

  ValidateMesh(input);
  if (!std::isfinite(threshold) || threshold < 0.0 ||
      (!real_metric && threshold > 1.0))
    throw std::invalid_argument("CoACD threshold is outside its valid range");
  if (max_convex_hull == 0 || max_convex_hull < -1 || sample_resolution <= 0 ||
      mcts_nodes <= 0 || mcts_iteration <= 0 || mcts_max_depth <= 0 ||
      max_ch_vertex <= 0)
    throw std::invalid_argument("CoACD count and resolution options must be positive");
  if (!std::isfinite(extrude_margin) || extrude_margin < 0.0)
    throw std::invalid_argument("CoACD extrusion margin must be finite and non-negative");
  if (preprocess_mode != "auto" && preprocess_mode != "on" && preprocess_mode != "off")
    throw std::invalid_argument("invalid CoACD preprocess mode");
  if (apx_mode != "ch" && apx_mode != "box")
    throw std::invalid_argument("invalid CoACD approximation mode");

  logger::info("threshold               {}", threshold);
  logger::info("max # convex hull       {}", max_convex_hull);
  logger::info("preprocess mode         {}", preprocess_mode);
  logger::info("preprocess resolution   {}", prep_resolution);
  logger::info("pca                     {}", pca);
  logger::info("mcts max depth          {}", mcts_max_depth);
  logger::info("mcts nodes              {}", mcts_nodes);
  logger::info("mcts iterations         {}", mcts_iteration);
  logger::info("merge                   {}", merge);
  logger::info("decimate                {}", decimate);
  logger::info("max_ch_vertex           {}", max_ch_vertex);
  logger::info("extrude                 {}", extrude);
  logger::info("extrude margin          {}", extrude_margin);
  logger::info("approximate mode        {}", apx_mode);
  logger::info("seed                    {}", seed);

  if (!real_metric && threshold > 1) {
    throw std::runtime_error("CoACD threshold > 1 (should be 0-1).");
  }

  if (prep_resolution > 1000) {
    throw std::runtime_error("CoACD prep resolution > 1000, this is probably a "
                             "bug (should be 30-100).");
  } else if (prep_resolution < 5) {
    throw std::runtime_error("CoACD prep resolution < 5, this is probably a "
                             "bug (should be 20-100).");
  }

  Params params;
  params.input_model = "";
  params.output_name = "";
  params.threshold = threshold;
  params.max_convex_hull = max_convex_hull;
  params.preprocess_mode = preprocess_mode;
  params.prep_resolution = prep_resolution;
  params.resolution = sample_resolution;
  params.mcts_nodes = mcts_nodes;
  params.mcts_iteration = mcts_iteration;
  params.mcts_max_depth = mcts_max_depth;
  params.pca = pca;
  params.merge = merge;
  params.decimate = decimate;
  params.max_ch_vertex = max_ch_vertex;
  params.extrude = extrude;
  params.extrude_margin = extrude_margin;
  params.apx_mode = apx_mode;
  params.seed = seed;
  params.real_metric = real_metric;
  params.cancellation_requested = cancellation_requested;

  CheckCancellation(params);

  Model m;
  m.Load(std::move(input.vertices), std::move(input.indices));
  vector<double> bbox = m.Normalize();
  CheckCancellation(params);

  if (real_metric) {
    double m_len = max(max(bbox[1] - bbox[0], bbox[3] - bbox[2]), bbox[5] - bbox[4]);
    double original_threshold = params.threshold;
    params.threshold = params.threshold * 2.0 / m_len * 0.8;
    logger::info("Real metric mode: mesh max length = {:.2f} cm", m_len * 100.0);
    logger::info("Real metric mode: error threshold = {:.2f} cm", original_threshold * 100.0);
    logger::info("Real metric mode: threshold {:.4f} cm (real) -> {:.4f} (normalized)", original_threshold * 100.0, params.threshold);
  }


  array<array<double, 3>, 3> rot{
      {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}};

#if WITH_3RD_PARTY_LIBS
  if (params.preprocess_mode == std::string("auto")) {
    bool is_manifold = IsManifold(m, &params);
    logger::info("Mesh Manifoldness: {}", is_manifold);
    if (!is_manifold)
      ManifoldPreprocess(params, m);
  } else if (params.preprocess_mode == std::string("on")) {
    ManifoldPreprocess(params, m);
  }
#else
  bool is_manifold = IsManifold(m, &params);
  if (!is_manifold)
    throw std::runtime_error("The mesh is not a 2-manifold!");
#endif

  if (pca) {
    rot = m.PCA();
  }

  CheckCancellation(params);
  vector<Model> parts = Compute(m, params);
  CheckCancellation(params);
  RecoverParts(parts, bbox, rot);

  std::vector<Mesh> result;
  result.reserve(parts.size());
  for (auto &p : parts) {
    result.push_back(Mesh{.vertices = std::move(p.points),
                          .indices = std::move(p.triangles)});
  }
  return result;
}

std::vector<Mesh> CoACD(Mesh const &input, double threshold,
                        int max_convex_hull, std::string preprocess_mode,
                        int prep_resolution, int sample_resolution,
                        int mcts_nodes, int mcts_iteration, int mcts_max_depth,
                        bool pca, bool merge, bool decimate, int max_ch_vertex,
                        bool extrude, double extrude_margin,
                        std::string apx_mode, unsigned int seed,
                        bool real_metric) {
  return CoACDWithCancellation(
      input, threshold, max_convex_hull, std::move(preprocess_mode),
      prep_resolution, sample_resolution, mcts_nodes, mcts_iteration,
      mcts_max_depth, pca, merge, decimate, max_ch_vertex, extrude,
      extrude_margin, std::move(apx_mode), seed, real_metric, nullptr);
}

void set_log_level(std::string_view level) {
#ifndef DISABLE_SPDLOG
  if (level == "off") {
    logger::get()->set_level(spdlog::level::off);
  } else if (level == "debug") {
    logger::get()->set_level(spdlog::level::debug);
  } else if (level == "info") {
    logger::get()->set_level(spdlog::level::info);
  } else if (level == "warn" || level == "warning") {
    logger::get()->set_level(spdlog::level::warn);
  } else if (level == "error" || level == "err") {
    logger::get()->set_level(spdlog::level::err);
  } else if (level == "critical") {
    logger::get()->set_level(spdlog::level::critical);
  } else {
    throw std::runtime_error("invalid log level " + std::string(level));
  }
#endif
}

} // namespace coacd

extern "C" {
void CoACD_freeMeshArray(CoACD_MeshArray arr) {
  if (arr.meshes_ptr == nullptr)
    return;
  for (uint64_t i = 0; i < arr.meshes_count; ++i) {
    delete[] arr.meshes_ptr[i].vertices_ptr;
    delete[] arr.meshes_ptr[i].triangles_ptr;
  }
  delete[] arr.meshes_ptr;
}

CoACD_MeshArray CoACD_run(CoACD_Mesh const &input, double threshold,
                          int max_convex_hull, int preprocess_mode,
                          int prep_resolution, int sample_resolution,
                          int mcts_nodes, int mcts_iteration,
                          int mcts_max_depth, bool pca, bool merge,
                          bool decimate, int max_ch_vertex,
                          bool extrude, double extrude_margin,
                          int apx_mode, unsigned int seed,
                          bool real_metric) {
  if ((input.vertices_count != 0 && input.vertices_ptr == nullptr) ||
      (input.triangles_count != 0 && input.triangles_ptr == nullptr))
    throw std::invalid_argument("CoACD input pointers must not be null");
  if (input.vertices_count > std::numeric_limits<std::size_t>::max() / 3 ||
      input.triangles_count > std::numeric_limits<std::size_t>::max() / 3)
    throw std::length_error("CoACD input mesh is too large");

  coacd::Mesh mesh;
  mesh.vertices.reserve(static_cast<std::size_t>(input.vertices_count));
  mesh.indices.reserve(static_cast<std::size_t>(input.triangles_count));
  for (uint64_t i = 0; i < input.vertices_count; ++i) {
    mesh.vertices.push_back({input.vertices_ptr[3 * i],
                             input.vertices_ptr[3 * i + 1],
                             input.vertices_ptr[3 * i + 2]});
  }
  for (uint64_t i = 0; i < input.triangles_count; ++i) {
    mesh.indices.push_back({input.triangles_ptr[3 * i],
                            input.triangles_ptr[3 * i + 1],
                            input.triangles_ptr[3 * i + 2]});
  }

  std::string pm, apx;
  if (preprocess_mode == preprocess_on) {
    pm = "on";
  } else if (preprocess_mode == preprocess_off) {
    pm = "off";
  } else {
    pm = "auto";
  }

  if (apx_mode == apx_ch) {
    apx = "ch";
  } else if (apx_mode == apx_box) {
    apx = "box";
  } else {
    throw std::runtime_error("invalid approximation mode " + std::to_string(apx_mode));
  }

  auto meshes = coacd::CoACD(mesh, threshold, max_convex_hull, pm,
                             prep_resolution, sample_resolution, mcts_nodes,
                             mcts_iteration, mcts_max_depth, pca, merge, decimate, max_ch_vertex,
                             extrude, extrude_margin, apx, seed, real_metric);

  CoACD_MeshArray arr{nullptr, 0};
  std::unique_ptr<CoACD_Mesh[]> mesh_storage(
      new CoACD_Mesh[meshes.size()]{});
  try {
    for (size_t i = 0; i < meshes.size(); ++i) {
      if (meshes[i].vertices.size() > std::numeric_limits<std::size_t>::max() / 3 ||
          meshes[i].indices.size() > std::numeric_limits<std::size_t>::max() / 3)
        throw std::length_error("CoACD output mesh is too large");

      std::unique_ptr<double[]> vertices(
          new double[meshes[i].vertices.size() * 3]);
      for (size_t j = 0; j < meshes[i].vertices.size(); ++j) {
        vertices[3 * j] = meshes[i].vertices[j][0];
        vertices[3 * j + 1] = meshes[i].vertices[j][1];
        vertices[3 * j + 2] = meshes[i].vertices[j][2];
      }
      std::unique_ptr<int[]> triangles(
          new int[meshes[i].indices.size() * 3]);
      for (size_t j = 0; j < meshes[i].indices.size(); ++j) {
        triangles[3 * j] = meshes[i].indices[j][0];
        triangles[3 * j + 1] = meshes[i].indices[j][1];
        triangles[3 * j + 2] = meshes[i].indices[j][2];
      }
      mesh_storage[i].vertices_count = meshes[i].vertices.size();
      mesh_storage[i].triangles_count = meshes[i].indices.size();
      mesh_storage[i].vertices_ptr = vertices.release();
      mesh_storage[i].triangles_ptr = triangles.release();
    }
  } catch (...) {
    for (size_t i = 0; i < meshes.size(); ++i) {
      delete[] mesh_storage[i].vertices_ptr;
      delete[] mesh_storage[i].triangles_ptr;
    }
    throw;
  }
  arr.meshes_count = meshes.size();
  arr.meshes_ptr = mesh_storage.release();
  return arr;
}

void CoACD_setLogLevel(char const *level) {
  if (level == nullptr)
    throw std::invalid_argument("CoACD log level must not be null");
  coacd::set_log_level(std::string_view(level));
}
}
