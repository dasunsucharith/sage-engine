#include "sage/world/scene_loader.h"

#include "sage/physics/physics_world.h"
#include "sage/world/world.h"

#include <nlohmann/json.hpp>
#include <stb_image.h> // Implementation lives in gl_renderer.cpp

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <string_view>

namespace sage {

namespace {

// ---------------------------------------------------------------------------
// Terrain JSON parsing with precise, actionable errors
// ---------------------------------------------------------------------------
// nlohmann::json has no line numbers, so every message names the scene file AND the
// exact JSON path (e.g. terrain.features[2].radius) so a human or AI can fix it fast.
class TerrainJsonReader {
public:
    explicit TerrainJsonReader(const std::filesystem::path& scenePath) : m_scene(scenePath.string()) {}

    bool ok() const { return m_errors == 0; }

    void error(const std::string& path, const std::string& msg) {
        ++m_errors;
        std::cerr << "[SAGE][Scene][Terrain] ERROR " << m_scene << " -> " << path << ": " << msg << "\n";
    }
    void warn(const std::string& path, const std::string& msg) const {
        std::cerr << "[SAGE][Scene][Terrain] WARNING " << m_scene << " -> " << path << ": " << msg << "\n";
    }

    // Warns about keys we don't understand (usually typos like "radious").
    void checkKeys(const nlohmann::json& obj, const std::string& path, std::initializer_list<std::string_view> allowed) const {
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            const bool known = std::find(allowed.begin(), allowed.end(), std::string_view(it.key())) != allowed.end();
            if (!known) {
                warn(path + "." + it.key(), "unknown key (ignored). Check the spelling against docs/terrain.md");
            }
        }
    }

    float num(const nlohmann::json& obj, const char* key, float fallback, const std::string& path) {
        if (!obj.contains(key)) return fallback;
        if (!obj[key].is_number()) {
            error(path + "." + key, "expected a number, got " + std::string(obj[key].type_name()));
            return fallback;
        }
        const float v = obj[key].get<float>();
        if (!std::isfinite(v)) {
            error(path + "." + key, "must be a finite number");
            return fallback;
        }
        return v;
    }

    int integer(const nlohmann::json& obj, const char* key, int fallback, const std::string& path) {
        if (!obj.contains(key)) return fallback;
        if (!obj[key].is_number_integer()) {
            error(path + "." + key, "expected a whole number, got " + obj[key].dump());
            return fallback;
        }
        return obj[key].get<int>();
    }

    bool boolean(const nlohmann::json& obj, const char* key, bool fallback, const std::string& path) {
        if (!obj.contains(key)) return fallback;
        if (!obj[key].is_boolean()) {
            error(path + "." + key, "expected true or false, got " + obj[key].dump());
            return fallback;
        }
        return obj[key].get<bool>();
    }

    std::string str(const nlohmann::json& obj, const char* key, const std::string& fallback, const std::string& path) {
        if (!obj.contains(key)) return fallback;
        if (!obj[key].is_string()) {
            error(path + "." + key, "expected a string, got " + obj[key].dump());
            return fallback;
        }
        return obj[key].get<std::string>();
    }

    Color3 color(const nlohmann::json& obj, const char* key, Color3 fallback, const std::string& path) {
        if (!obj.contains(key)) return fallback;
        const auto& c = obj[key];
        if (!c.is_array() || c.size() != 3 || !c[0].is_number() || !c[1].is_number() || !c[2].is_number()) {
            error(path + "." + key, "expected [r, g, b] with numbers 0..1 (linear RGB), got " + c.dump());
            return fallback;
        }
        Color3 out{c[0].get<float>(), c[1].get<float>(), c[2].get<float>()};
        if (out.r > 1.0f || out.g > 1.0f || out.b > 1.0f || out.r < 0.0f || out.g < 0.0f || out.b < 0.0f) {
            warn(path + "." + key, "color components should be 0..1 (did you use 0..255?). Values were clamped");
            out = {std::clamp(out.r, 0.0f, 1.0f), std::clamp(out.g, 0.0f, 1.0f), std::clamp(out.b, 0.0f, 1.0f)};
        }
        return out;
    }

private:
    std::string m_scene;
    int m_errors = 0;
};

// Resolves an asset path relative to the working dir, the project root (scene's
// parent folder's parent), or the scene's own folder, in that order.
std::filesystem::path resolveSceneAsset(const std::filesystem::path& scenePath, const std::string& rel) {
    namespace fs = std::filesystem;
    const fs::path p(rel);
    std::error_code ec;
    if (fs::exists(p, ec)) return p;
    const fs::path fromRoot = scenePath.parent_path().parent_path() / p;
    if (fs::exists(fromRoot, ec)) return fromRoot;
    const fs::path fromScene = scenePath.parent_path() / p;
    if (fs::exists(fromScene, ec)) return fromScene;
    return {};
}

bool loadHeightmapImage(const std::filesystem::path& scenePath, TerrainSpec& spec, TerrainJsonReader& r) {
    const std::filesystem::path resolved = resolveSceneAsset(scenePath, spec.heightmapPath);
    if (resolved.empty()) {
        r.error("terrain.heightmap.path", "file not found: \"" + spec.heightmapPath +
            "\" (looked relative to the working folder, the project root and the scene folder)");
        return false;
    }
    int w = 0, h = 0, channels = 0;
    // stbi_load_16 accepts 8-bit and 16-bit images and returns 16-bit samples; 1 = grayscale.
    stbi_us* pixels = stbi_load_16(resolved.string().c_str(), &w, &h, &channels, 1);
    if (!pixels) {
        r.error("terrain.heightmap.path", "could not decode \"" + resolved.string() + "\": " +
            (stbi_failure_reason() ? stbi_failure_reason() : "unknown error") + " (use an 8- or 16-bit grayscale PNG)");
        return false;
    }
    if (w < 2 || h < 2 || w > 8193 || h > 8193) {
        stbi_image_free(pixels);
        r.error("terrain.heightmap.path", "image must be between 2x2 and 8193x8193 pixels, got " +
            std::to_string(w) + "x" + std::to_string(h));
        return false;
    }
    spec.heightmapWidth = w;
    spec.heightmapHeight = h;
    spec.heightmap.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    // Image row 0 is the TOP of the picture = the far (+Z / north) edge, so flip rows
    // to make the heightmap look like a top-down map with north up.
    for (int row = 0; row < h; ++row) {
        const int dstRow = h - 1 - row;
        for (int col = 0; col < w; ++col) {
            spec.heightmap[static_cast<std::size_t>(dstRow) * static_cast<std::size_t>(w) + static_cast<std::size_t>(col)] =
                static_cast<float>(pixels[static_cast<std::size_t>(row) * static_cast<std::size_t>(w) + static_cast<std::size_t>(col)]) / 65535.0f;
        }
    }
    stbi_image_free(pixels);
    std::cout << "[SAGE][Terrain] Loaded heightmap " << resolved.string() << " (" << w << "x" << h << ", "
              << spec.heightmapMin << "m .. " << spec.heightmapMax << "m)\n";
    return true;
}

bool parseTerrain(const nlohmann::json& t, const std::filesystem::path& scenePath, TerrainSettings& out) {
    TerrainJsonReader r(scenePath);
    const std::string P = "terrain";
    out.sourceJson = t.dump();

    r.checkKeys(t, P, {"enabled", "type", "mode", "preset", "halfSize", "maxHeight", "gridResolution",
                       "baseHeight", "heightmap", "noise", "island", "features", "layers", "cliff", "snow",
                       "grass", "collisionMinHeight", "hideBelowHeight"});

    out.enabled = r.boolean(t, "enabled", true, P);
    out.halfSize = r.num(t, "halfSize", out.halfSize, P);
    out.maxHeight = r.num(t, "maxHeight", out.maxHeight, P);
    out.gridResolution = r.integer(t, "gridResolution", out.gridResolution, P);
    if (out.halfSize < 10.0f || out.halfSize > 4000.0f) {
        r.error(P + ".halfSize", "must be between 10 and 4000 meters (terrain spans -halfSize..+halfSize)");
    }
    if (out.gridResolution < 32 || out.gridResolution > 240) {
        r.warn(P + ".gridResolution", "clamped to 32..240 render grid cells per side");
        out.gridResolution = std::clamp(out.gridResolution, 32, 240);
    }

    // Which terrain? New scenes use "type"; the legacy "mode": "island"/"city" still works.
    const std::string type = r.str(t, "type", "", P);
    const std::string preset = r.str(t, "preset", "", P);
    const std::string legacyMode = r.str(t, "mode", "", P);
    if (type == "procedural" || type == "heightmap") {
        out.mode = TerrainSettings::kModeDataDriven;
    } else if (type == "preset" || type.empty()) {
        const std::string which = !preset.empty() ? preset : legacyMode;
        if (which == "echo_island" || which == "island" || which == "wilderness") {
            out.mode = TerrainSettings::kModeEchoIslandPreset;
        } else if (which.empty() || which == "stunt_city" || which == "city") {
            out.mode = TerrainSettings::kModeStuntCityPreset;
        } else {
            r.error(P + ".preset", "unknown preset \"" + which + "\" (use \"stunt_city\" or \"echo_island\")");
        }
    } else {
        r.error(P + ".type", "unknown terrain type \"" + type + "\" (use \"procedural\", \"heightmap\" or \"preset\")");
    }
    if (out.mode != TerrainSettings::kModeDataDriven) {
        return r.ok(); // Presets are fully defined in C++; ignore the data-driven keys.
    }

    TerrainSpec& s = out.spec;
    s.baseHeight = r.num(t, "baseHeight", s.baseHeight, P);
    s.collisionMinHeight = r.num(t, "collisionMinHeight", s.collisionMinHeight, P);
    s.hideBelowHeight = r.num(t, "hideBelowHeight", s.hideBelowHeight, P);

    if (t.contains("heightmap")) {
        const auto& hm = t["heightmap"];
        const std::string HP = P + ".heightmap";
        if (!hm.is_object()) {
            r.error(HP, "expected an object like {\"path\": \"assets/terrain/map.png\", \"minHeight\": -5, \"maxHeight\": 60}");
        } else {
            r.checkKeys(hm, HP, {"path", "minHeight", "maxHeight"});
            s.heightmapPath = r.str(hm, "path", "", HP);
            s.heightmapMin = r.num(hm, "minHeight", s.heightmapMin, HP);
            s.heightmapMax = r.num(hm, "maxHeight", s.heightmapMax, HP);
            if (s.heightmapPath.empty()) {
                r.error(HP + ".path", "missing image path");
            } else {
                loadHeightmapImage(scenePath, s, r);
            }
        }
    } else if (type == "heightmap") {
        r.error(P + ".heightmap", "type is \"heightmap\" but no heightmap {\"path\": ...} block was given");
    }

    if (t.contains("noise")) {
        const auto& n = t["noise"];
        const std::string NP = P + ".noise";
        if (!n.is_object()) {
            r.error(NP, "expected an object");
        } else {
            r.checkKeys(n, NP, {"amplitude", "frequency", "octaves", "ridged", "warp", "seed"});
            s.noiseAmplitude = r.num(n, "amplitude", 12.0f, NP);
            s.noiseFrequency = r.num(n, "frequency", s.noiseFrequency, NP);
            s.noiseOctaves = std::clamp(r.integer(n, "octaves", s.noiseOctaves, NP), 1, 8);
            s.noiseRidged = std::clamp(r.num(n, "ridged", s.noiseRidged, NP), 0.0f, 1.0f);
            s.noiseWarp = r.num(n, "warp", s.noiseWarp, NP);
            s.noiseSeed = r.num(n, "seed", s.noiseSeed, NP);
            if (s.noiseFrequency <= 0.0f || s.noiseFrequency > 1.0f) {
                r.error(NP + ".frequency", "must be > 0 and <= 1 (0.01 = hills about 100 m apart)");
            }
        }
    }

    if (t.contains("island")) {
        const auto& is = t["island"];
        const std::string IP = P + ".island";
        if (!is.is_object()) {
            r.error(IP, "expected an object");
        } else {
            r.checkKeys(is, IP, {"center", "radius", "falloff", "coastNoise", "seabedHeight"});
            s.island = true;
            if (is.contains("center")) {
                const auto& c = is["center"];
                if (c.is_array() && c.size() == 2 && c[0].is_number() && c[1].is_number()) {
                    s.islandCenterX = c[0].get<float>();
                    s.islandCenterZ = c[1].get<float>();
                } else {
                    r.error(IP + ".center", "expected [x, z]");
                }
            }
            s.islandRadius = r.num(is, "radius", s.islandRadius, IP);
            s.islandFalloff = std::max(r.num(is, "falloff", s.islandFalloff, IP), 1.0f);
            s.islandCoastNoise = std::max(r.num(is, "coastNoise", s.islandCoastNoise, IP), 0.0f);
            s.seabedHeight = r.num(is, "seabedHeight", s.seabedHeight, IP);
        }
    }

    if (t.contains("features")) {
        const auto& fs = t["features"];
        if (!fs.is_array()) {
            r.error(P + ".features", "expected an array of feature objects");
        } else {
            for (std::size_t i = 0; i < fs.size(); ++i) {
                const auto& f = fs[i];
                const std::string FP = P + ".features[" + std::to_string(i) + "]";
                if (!f.is_object()) {
                    r.error(FP, "expected an object");
                    continue;
                }
                TerrainFeature feat;
                const std::string ft = r.str(f, "type", "", FP);
                if (ft == "peak") feat.type = TerrainFeature::Type::Peak;
                else if (ft == "basin" || ft == "lake" || ft == "crater") feat.type = TerrainFeature::Type::Basin;
                else if (ft == "flatten" || ft == "plateau") feat.type = TerrainFeature::Type::Flatten;
                else if (ft == "road") feat.type = TerrainFeature::Type::Road;
                else {
                    r.error(FP + ".type", "unknown feature \"" + ft + "\" (use peak, basin, flatten or road)");
                    continue;
                }
                r.checkKeys(f, FP, {"type", "name", "center", "radius", "innerRadius", "height", "depth",
                                    "strength", "sharpness", "points", "width", "blend"});
                if (feat.type == TerrainFeature::Type::Road) {
                    const auto& pts = f.contains("points") ? f["points"] : nlohmann::json();
                    if (!pts.is_array() || pts.size() < 2) {
                        r.error(FP + ".points", "a road needs at least 2 points: [[x, z], [x, z], ...]");
                        continue;
                    }
                    feat.width = std::max(r.num(f, "width", feat.width, FP), 0.5f);
                    feat.blend = std::max(r.num(f, "blend", feat.blend, FP), 0.0f);
                    feat.height = r.num(f, "height", 0.0f, FP);
                    bool good = true;
                    for (const auto& p : pts) {
                        const bool xz = p.is_array() && (p.size() == 2 || p.size() == 3) && p[0].is_number() && p[1].is_number();
                        if (!xz || (p.size() == 3 && !p[2].is_number())) {
                            good = false;
                            break;
                        }
                        feat.points.push_back(p[0].get<float>());
                        feat.points.push_back(p[1].get<float>());
                        feat.points.push_back(p.size() == 3 ? p[2].get<float>() : feat.height);
                    }
                    if (!good) {
                        r.error(FP + ".points", "every point must be [x, z] or [x, z, height]");
                        continue;
                    }
                } else {
                    if (!f.contains("center")) {
                        r.error(FP + ".center", "missing [x, z] center");
                        continue;
                    }
                    const auto& c = f["center"];
                    if (!c.is_array() || c.size() != 2 || !c[0].is_number() || !c[1].is_number()) {
                        r.error(FP + ".center", "expected [x, z]");
                        continue;
                    }
                    feat.x = c[0].get<float>();
                    feat.z = c[1].get<float>();
                    feat.radius = r.num(f, "radius", feat.radius, FP);
                    if (feat.radius <= 0.0f) {
                        r.error(FP + ".radius", "must be greater than 0");
                        continue;
                    }
                    feat.innerRadius = std::clamp(r.num(f, "innerRadius", feat.radius * 0.35f, FP), 0.0f, feat.radius * 0.999f);
                    feat.height = r.num(f, "height", feat.height, FP);
                    feat.depth = r.num(f, "depth", feat.depth, FP);
                    feat.strength = std::clamp(r.num(f, "strength", feat.strength, FP), 0.0f, 1.0f);
                    feat.sharpness = std::max(r.num(f, "sharpness", feat.sharpness, FP), 0.1f);
                }
                s.features.push_back(std::move(feat));
            }
        }
    }

    if (t.contains("layers")) {
        const auto& ls = t["layers"];
        if (!ls.is_array()) {
            r.error(P + ".layers", "expected an array of {\"name\", \"color\", \"startHeight\"} objects");
        } else {
            for (std::size_t i = 0; i < ls.size(); ++i) {
                const auto& l = ls[i];
                const std::string LP = P + ".layers[" + std::to_string(i) + "]";
                if (!l.is_object()) {
                    r.error(LP, "expected an object");
                    continue;
                }
                r.checkKeys(l, LP, {"name", "color", "startHeight", "blend", "roughness"});
                TerrainLayer layer;
                layer.name = r.str(l, "name", "layer" + std::to_string(i), LP);
                layer.color = r.color(l, "color", layer.color, LP);
                layer.startHeight = r.num(l, "startHeight", i == 0 ? -1.0e6f : 0.0f, LP);
                layer.blend = std::max(r.num(l, "blend", layer.blend, LP), 0.01f);
                layer.roughness = std::clamp(r.num(l, "roughness", layer.roughness, LP), 0.04f, 1.0f);
                s.layers.push_back(std::move(layer));
            }
            std::stable_sort(s.layers.begin(), s.layers.end(), [](const TerrainLayer& a, const TerrainLayer& b) {
                return a.startHeight < b.startHeight;
            });
            if (s.layers.size() > 6) {
                r.warn(P + ".layers", "only the lowest 6 layers are rendered (" + std::to_string(s.layers.size()) + " given)");
            }
        }
    }
    if (s.layers.empty()) {
        // Sensible default palette so a bare {"type": "procedural"} still looks like land.
        s.layers = {
            {"sand", {0.50f, 0.42f, 0.27f}, -1.0e6f, 1.5f, 0.80f},
            {"grass", {0.10f, 0.22f, 0.07f}, 2.5f, 2.5f, 0.88f},
            {"highland", {0.17f, 0.17f, 0.11f}, 18.0f, 6.0f, 0.90f},
        };
    }

    if (t.contains("cliff")) {
        const auto& c = t["cliff"];
        const std::string CP = P + ".cliff";
        if (!c.is_object()) {
            r.error(CP, "expected an object");
            return false;
        }
        r.checkKeys(c, CP, {"color", "slopeStart", "slopeEnd"});
        s.cliffColor = r.color(c, "color", s.cliffColor, CP);
        s.cliffSlopeStart = std::clamp(r.num(c, "slopeStart", s.cliffSlopeStart, CP), 0.0f, 1.0f);
        s.cliffSlopeEnd = std::clamp(r.num(c, "slopeEnd", s.cliffSlopeEnd, CP), s.cliffSlopeStart + 0.01f, 1.0f);
    }
    if (t.contains("snow")) {
        const auto& sn = t["snow"];
        const std::string SP = P + ".snow";
        if (!sn.is_object()) {
            r.error(SP, "expected an object");
            return false;
        }
        r.checkKeys(sn, SP, {"enabled", "color", "startHeight", "endHeight"});
        s.snowEnabled = r.boolean(sn, "enabled", true, SP);
        s.snowColor = r.color(sn, "color", s.snowColor, SP);
        s.snowStartHeight = r.num(sn, "startHeight", s.snowStartHeight, SP);
        s.snowEndHeight = std::max(r.num(sn, "endHeight", s.snowStartHeight + 8.0f, SP), s.snowStartHeight + 0.1f);
    }
    if (t.contains("grass")) {
        const auto& g = t["grass"];
        const std::string GP = P + ".grass";
        if (!g.is_object()) {
            r.error(GP, "expected an object");
            return false;
        }
        r.checkKeys(g, GP, {"enabled", "count", "area", "minHeight", "maxHeight", "minNormalY", "avoidFeatures"});
        s.grassEnabled = r.boolean(g, "enabled", s.grassEnabled, GP);
        s.grassCount = std::clamp(r.integer(g, "count", s.grassCount, GP), 0, 60000);
        s.grassArea = std::clamp(r.num(g, "area", s.grassArea, GP), 0.05f, 1.0f);
        s.grassMinHeight = r.num(g, "minHeight", s.grassMinHeight, GP);
        s.grassMaxHeight = r.num(g, "maxHeight", s.grassMaxHeight, GP);
        s.grassMinNormalY = std::clamp(r.num(g, "minNormalY", s.grassMinNormalY, GP), 0.0f, 1.0f);
        s.grassAvoidFeatures = r.boolean(g, "avoidFeatures", s.grassAvoidFeatures, GP);
    }

    std::cout << "[SAGE][Terrain] Data-driven terrain: +/-" << out.halfSize << "m, "
              << s.features.size() << " feature(s), " << s.layers.size() << " layer(s)"
              << (s.heightmap.empty() ? "" : ", heightmap") << (s.island ? ", island" : "") << "\n";
    return r.ok();
}

Vec3 readVec3(const nlohmann::json& obj, const char* key, Vec3 fallback) {
    if (!obj.contains(key) || !obj[key].is_array() || obj[key].size() < 3) {
        return fallback;
    }
    return {
        obj[key][0].get<float>(),
        obj[key][1].get<float>(),
        obj[key][2].get<float>(),
    };
}

Color3 readColor3(const nlohmann::json& obj, const char* key, Color3 fallback) {
    if (!obj.contains(key) || !obj[key].is_array() || obj[key].size() < 3) {
        return fallback;
    }
    return {
        obj[key][0].get<float>(),
        obj[key][1].get<float>(),
        obj[key][2].get<float>(),
    };
}

} // namespace

bool SceneLoader::loadFromFile(const std::filesystem::path& scenePath, World& world, PhysicsWorld* physics) {
    if (!std::filesystem::exists(scenePath)) {
        std::cerr << "[SAGE][Scene] Scene file not found: " << scenePath.string() << "\n";
        return false;
    }

    std::ifstream file(scenePath);
    if (!file.is_open()) {
        std::cerr << "[SAGE][Scene] Failed to open scene file: " << scenePath.string() << "\n";
        return false;
    }

    nlohmann::json root;
    try {
        file >> root;
    } catch (const nlohmann::json::exception& ex) {
        std::cerr << "[SAGE][Scene] JSON parse error in " << scenePath.string()
                  << ":\n  " << ex.what() << "\n";
        return false;
    }

    const std::string sceneName = root.value("name", scenePath.stem().string());

    // Every scene load starts from a clean default terrain, so hot-reloading from a
    // scene WITH a terrain block to one WITHOUT never leaves stale terrain behind.
    {
        const std::uint32_t nextRevision = world.terrain().revision + 1;
        world.terrain() = TerrainSettings{};
        world.terrain().revision = nextRevision;
    }
    if (root.contains("terrain") && root["terrain"].is_object()) {
        if (!parseTerrain(root["terrain"], scenePath, world.terrain())) {
            std::cerr << "[SAGE][Scene] Terrain block in " << scenePath.string()
                      << " has errors (see above); falling back to flat terrain.\n";
            const std::uint32_t rev = world.terrain().revision;
            world.terrain() = TerrainSettings{};
            world.terrain().enabled = false;
            world.terrain().revision = rev + 1;
        }
    }

    if (root.contains("sky") && root["sky"].is_object()) {
        const auto& skyJson = root["sky"];
        if (skyJson.contains("timeOfDay") && skyJson["timeOfDay"].is_number()) {
            world.setTimeOfDay(skyJson["timeOfDay"].get<float>());
        }
        if (skyJson.contains("sunColor")) {
            world.sky().sunColor = readColor3(skyJson, "sunColor", world.sky().sunColor);
        }
        if (skyJson.contains("skyZenithColor")) {
            world.sky().skyZenithColor = readColor3(skyJson, "skyZenithColor", world.sky().skyZenithColor);
        }
        if (skyJson.contains("skyHorizonColor")) {
            world.sky().skyHorizonColor = readColor3(skyJson, "skyHorizonColor", world.sky().skyHorizonColor);
        }
        if (skyJson.contains("ambientIntensity") && skyJson["ambientIntensity"].is_number()) {
            world.sky().ambientIntensity = skyJson["ambientIntensity"].get<float>();
        }
        if (skyJson.contains("fogStart") && skyJson["fogStart"].is_number()) {
            world.sky().fogStart = skyJson["fogStart"].get<float>();
        }
        if (skyJson.contains("fogEnd") && skyJson["fogEnd"].is_number()) {
            world.sky().fogEnd = skyJson["fogEnd"].get<float>();
        }
        if (skyJson.contains("rainIntensity") && skyJson["rainIntensity"].is_number()) {
            world.sky().rainIntensity = skyJson["rainIntensity"].get<float>();
        }
        if (skyJson.contains("wetness") && skyJson["wetness"].is_number()) {
            world.sky().wetness = skyJson["wetness"].get<float>();
        }
    }

    if (root.contains("camera") && root["camera"].is_object()) {
        const auto& camJson = root["camera"];
        world.camera().position = readVec3(camJson, "position", world.camera().position);
        world.camera().target = readVec3(camJson, "target", world.camera().target);
        world.camera().fovDegrees = camJson.value("fov", world.camera().fovDegrees);
    }

    std::size_t loadedCount = 0;
    std::size_t colliderCount = 0;
    if (root.contains("entities") && root["entities"].is_array()) {
        for (std::size_t i = 0; i < root["entities"].size(); ++i) {
            const auto& e = root["entities"][i];
            if (!e.is_object()) {
                continue;
            }

            const std::string id = e.value("id", "scene_entity_" + std::to_string(i));
            const std::string type = e.value("type", "primitive_box");
            Vec3 pos = readVec3(e, "position", {0.0f, 0.0f, 0.0f});
            const Vec3 rot = readVec3(e, "rotation", {0.0f, 0.0f, 0.0f});
            const Vec3 scale = readVec3(e, "scale", {1.0f, 1.0f, 1.0f});
            const bool emissive = e.value("emissive", false);
            const bool visible = e.value("visible", true);

            // Auto-ground off-road 3D models cleanly onto sculpted 3D terrain hills using 5-point footprint sampling
            // so buildings, boulders & trees embed firmly on steep slopes and NEVER float in mid-air!
            if (type == "model" && world.terrain().enabled && std::abs(pos.y) < 0.05f) {
                const float hc = world.sampleTerrain(pos.x, pos.z).height;
                if (world.terrain().mode != TerrainSettings::kModeStuntCityPreset) {
                    const float footR = std::clamp(std::max(scale.x, scale.z) * 1.45f, 0.8f, 3.6f);
                    const float hMin = std::min({
                        hc,
                        world.sampleTerrain(pos.x + footR, pos.z).height,
                        world.sampleTerrain(pos.x - footR, pos.z).height,
                        world.sampleTerrain(pos.x, pos.z + footR).height,
                        world.sampleTerrain(pos.x, pos.z - footR).height
                    });
                    // Sink gently toward lowest edge of footprint so slopes never expose floating undersides!
                    pos.y += (hMin * 0.78f + hc * 0.22f) - 0.14f;
                } else if (hc > 0.05f) {
                    pos.y += hc;
                }
            }

            if (type == "model") {
                const std::string modelPath = e.value("model", "");
                if (modelPath.empty()) {
                    std::cerr << "[SAGE][Scene] Entity \"" << id << "\" in " << scenePath.string()
                              << " has type=\"model\" but is missing \"model\" path.\n";
                    continue;
                }
                world.spawnModel(id, modelPath, pos, scale);
                if (e.contains("color")) {
                    const Color3 tint = readColor3(e, "color", {1.0f, 1.0f, 1.0f});
                    world.setColor(id, tint.r, tint.g, tint.b);
                }
            } else {
                EntityShape shape = EntityShape::Box;
                if (type == "primitive_plane") {
                    shape = EntityShape::Plane;
                } else if (type == "water") {
                    shape = EntityShape::Water;
                } else if (type == "primitive_sphere") {
                    shape = EntityShape::Sphere;
                } else if (type == "primitive_ring") {
                    shape = EntityShape::Ring;
                } else if (type == "primitive_triangle") {
                    shape = EntityShape::Triangle;
                }
                const Color3 color = readColor3(e, "color", {0.8f, 0.8f, 0.8f});
                world.spawnEntity(id, shape, pos, scale, color);
                if (shape == EntityShape::Water && e.contains("seabedY") && e["seabedY"].is_number()) {
                    world.setWaterSeabed(id, e["seabedY"].get<float>());
                }
            }

            world.setRotation(id, rot.x, rot.y, rot.z);
            world.setEmissive(id, emissive);
            world.setVisible(id, visible);
            ++loadedCount;

            // Optional declarative collider in JSON — automatically solid for Jolt AND vehicles!
            if (physics && e.contains("collider") && e["collider"].is_object()) {
                const auto& col = e["collider"];
                const std::string colType = col.value("type", "box");
                const float offsetY = col.value("offsetY", 0.0f);
                const std::string colId = "scene_" + id;

                if (colType == "cylinder") {
                    const float radius = col.value("radius", 0.6f);
                    const float height = col.value("height", 8.0f);
                    physics->addStaticCylinder(colId, {pos.x, pos.y + offsetY, pos.z}, radius, height);
                    ++colliderCount;
                } else if (colType == "ramp") {
                    const float w = col.value("width", 5.0f * scale.x);
                    const float h = col.value("height", 2.2f * scale.y);
                    const float l = col.value("length", 7.5f * scale.z);
                    physics->addStaticRamp(colId, {pos.x, pos.y + offsetY, pos.z}, w, h, l, rot.y);
                    ++colliderCount;
                } else if (colType == "deck") {
                    const Vec3 he = readVec3(col, "halfExtents", {scale.x * 0.5f, scale.y * 0.5f, scale.z * 0.5f});
                    physics->addStaticDeck(colId, {pos.x, pos.y + offsetY, pos.z}, he, rot.y);
                    ++colliderCount;
                } else {
                    const Vec3 he = readVec3(col, "halfExtents", {scale.x * 0.5f, scale.y * 0.5f, scale.z * 0.5f});
                    physics->addStaticBox(colId, {pos.x, pos.y + offsetY, pos.z}, he, rot);
                    ++colliderCount;
                }
            }
        }
    }

    // Optional declarative dynamic point lights in JSON ("lights": [...])
    std::size_t lightCount = 0;
    if (root.contains("lights") && root["lights"].is_array()) {
        for (std::size_t i = 0; i < root["lights"].size(); ++i) {
            const auto& l = root["lights"][i];
            if (!l.is_object()) {
                continue;
            }
            const std::string lid = l.value("id", "scene_light_" + std::to_string(i));
            const Vec3 pos = readVec3(l, "position", {0.0f, 3.0f, 0.0f});
            const Color3 col = readColor3(l, "color", {1.0f, 0.9f, 0.7f});
            const float radius = l.value("radius", 18.0f);
            const float intensity = l.value("intensity", 2.0f);
            world.setPointLight(lid, pos, col, radius, intensity);
            ++lightCount;
        }
    }

    if (physics && world.terrain().enabled) {
        physics->buildTerrainCollider(world);
    }

    std::cout << "[SAGE][Scene] Loaded \"" << sceneName << "\" from " << scenePath.string()
              << " (" << loadedCount << " entities, " << colliderCount << " solid collider(s), "
              << lightCount << " dynamic light(s))\n";
    return true;
}

bool SceneLoader::saveToFile(
    const std::filesystem::path& scenePath,
    const World& world,
    const std::string& sceneName) {
    nlohmann::json root;
    root["name"] = sceneName;
    root["version"] = 2;

    // Round-trip the terrain exactly as authored (data-driven specs can be large and
    // reference heightmap files, so re-serializing parsed values would lose intent).
    const TerrainSettings& terrain = world.terrain();
    if (!terrain.sourceJson.empty()) {
        root["terrain"] = nlohmann::json::parse(terrain.sourceJson, nullptr, false);
        if (root["terrain"].is_discarded()) {
            root.erase("terrain");
        } else {
            root["terrain"]["enabled"] = terrain.enabled; // Reflect the F1 editor toggle
        }
    } else if (terrain.mode == TerrainSettings::kModeEchoIslandPreset) {
        root["terrain"] = {{"enabled", terrain.enabled}, {"type", "preset"}, {"preset", "echo_island"},
                           {"halfSize", terrain.halfSize}, {"maxHeight", terrain.maxHeight},
                           {"gridResolution", terrain.gridResolution}};
    } else if (!terrain.enabled) {
        root["terrain"] = {{"enabled", false}};
    }

    const SkySettings& sky = world.sky();
    root["sky"] = {
        {"timeOfDay", sky.timeOfDay},
        {"ambientIntensity", sky.ambientIntensity},
        {"fogStart", sky.fogStart},
        {"fogEnd", sky.fogEnd},
        {"rainIntensity", sky.rainIntensity},
        {"wetness", sky.wetness},
    };

    const CameraState& cam = world.camera();
    root["camera"] = {
        {"position", {cam.position.x, cam.position.y, cam.position.z}},
        {"target", {cam.target.x, cam.target.y, cam.target.z}},
        {"fov", cam.fovDegrees},
    };

    nlohmann::json lightsArr = nlohmann::json::array();
    for (const PointLight& pl : world.pointLights()) {
        lightsArr.push_back({
            {"id", pl.id},
            {"position", {pl.position.x, pl.position.y, pl.position.z}},
            {"color", {pl.color.r, pl.color.g, pl.color.b}},
            {"radius", pl.radius},
            {"intensity", pl.intensity},
        });
    }
    root["lights"] = std::move(lightsArr);

    nlohmann::json entitiesArr = nlohmann::json::array();
    for (const Entity& e : world.entities()) {
        nlohmann::json obj;
        obj["id"] = e.id;
        switch (e.shape) {
            case EntityShape::Model:
                obj["type"] = "model";
                obj["model"] = e.modelPath;
                break;
            case EntityShape::Plane: obj["type"] = "primitive_plane"; break;
            case EntityShape::Water:
                obj["type"] = "water";
                obj["seabedY"] = e.waterSeabedY;
                break;
            case EntityShape::Sphere: obj["type"] = "primitive_sphere"; break;
            case EntityShape::Ring: obj["type"] = "primitive_ring"; break;
            case EntityShape::Triangle: obj["type"] = "primitive_triangle"; break;
            case EntityShape::Box:
            default: obj["type"] = "primitive_box"; break;
        }
        obj["position"] = {e.position.x, e.position.y, e.position.z};
        obj["rotation"] = {e.rotation.x, e.rotation.y, e.rotation.z};
        obj["scale"] = {e.scale.x, e.scale.y, e.scale.z};
        obj["color"] = {e.color.r, e.color.g, e.color.b};
        if (e.emissive) obj["emissive"] = true;
        if (!e.visible) obj["visible"] = false;
        entitiesArr.push_back(std::move(obj));
    }
    root["entities"] = std::move(entitiesArr);

    std::error_code ec;
    if (scenePath.has_parent_path()) {
        std::filesystem::create_directories(scenePath.parent_path(), ec);
    }
    std::ofstream out(scenePath);
    if (!out.is_open()) {
        std::cerr << "[SAGE][Scene] Failed to save scene file: " << scenePath.string() << "\n";
        return false;
    }
    out << root.dump(2) << "\n";
    std::cout << "[SAGE][Scene] Saved \"" << sceneName << "\" -> " << scenePath.string()
              << " (" << world.entityCount() << " entities, " << world.pointLights().size() << " lights)\n";
    return true;
}

} // namespace sage
