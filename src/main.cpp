#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace mc {

constexpr int kChunkSize = 16;
constexpr int kWorldHeight = 64;

struct Vec3i {
    int x{};
    int y{};
    int z{};

    auto operator<=>(const Vec3i&) const = default;
};

struct Vec3Hash {
    std::size_t operator()(const Vec3i& v) const noexcept {
        const std::size_t h1 = std::hash<int>{}(v.x);
        const std::size_t h2 = std::hash<int>{}(v.y);
        const std::size_t h3 = std::hash<int>{}(v.z);
        return h1 ^ (h2 << 1u) ^ (h3 << 2u);
    }
};

enum class BlockType : std::uint8_t {
    Air = 0,
    Grass,
    Dirt,
    Stone,
    Wood,
    Leaves,
    Sand,
    Water,
};

std::string blockName(BlockType block) {
    switch (block) {
        case BlockType::Air: return "air";
        case BlockType::Grass: return "grass";
        case BlockType::Dirt: return "dirt";
        case BlockType::Stone: return "stone";
        case BlockType::Wood: return "wood";
        case BlockType::Leaves: return "leaves";
        case BlockType::Sand: return "sand";
        case BlockType::Water: return "water";
    }
    return "unknown";
}

char blockGlyph(BlockType block) {
    switch (block) {
        case BlockType::Air: return '.';
        case BlockType::Grass: return 'G';
        case BlockType::Dirt: return 'D';
        case BlockType::Stone: return 'S';
        case BlockType::Wood: return 'W';
        case BlockType::Leaves: return 'L';
        case BlockType::Sand: return 'A';
        case BlockType::Water: return '~';
    }
    return '?';
}

std::optional<BlockType> parseBlock(const std::string& token) {
    if (token == "air") return BlockType::Air;
    if (token == "grass") return BlockType::Grass;
    if (token == "dirt") return BlockType::Dirt;
    if (token == "stone") return BlockType::Stone;
    if (token == "wood") return BlockType::Wood;
    if (token == "leaves") return BlockType::Leaves;
    if (token == "sand") return BlockType::Sand;
    if (token == "water") return BlockType::Water;
    return std::nullopt;
}

class Chunk {
public:
    explicit Chunk(Vec3i coord) : coord_(coord), blocks_(kChunkSize * kWorldHeight * kChunkSize, BlockType::Air) {}

    [[nodiscard]] Vec3i coord() const { return coord_; }

    [[nodiscard]] BlockType get(int localX, int y, int localZ) const {
        if (!inBounds(localX, y, localZ)) {
            return BlockType::Air;
        }
        return blocks_[index(localX, y, localZ)];
    }

    void set(int localX, int y, int localZ, BlockType value) {
        if (!inBounds(localX, y, localZ)) {
            return;
        }
        blocks_[index(localX, y, localZ)] = value;
    }

private:
    static bool inBounds(int localX, int y, int localZ) {
        return localX >= 0 && localX < kChunkSize && localZ >= 0 && localZ < kChunkSize && y >= 0 && y < kWorldHeight;
    }

    static std::size_t index(int localX, int y, int localZ) {
        return static_cast<std::size_t>((y * kChunkSize + localZ) * kChunkSize + localX);
    }

    Vec3i coord_;
    std::vector<BlockType> blocks_;
};

class World {
public:
    explicit World(std::uint32_t seed) : seed_(seed), noiseRng_(seed) {}

    BlockType getBlock(int x, int y, int z) {
        auto* chunk = ensureChunk(x, z);
        const auto local = toLocal(x, z);
        return chunk->get(local.first, y, local.second);
    }

    void setBlock(int x, int y, int z, BlockType type) {
        auto* chunk = ensureChunk(x, z);
        const auto local = toLocal(x, z);
        chunk->set(local.first, y, local.second, type);
    }

    int terrainHeight(int x, int z) const {
        const double rough = std::sin((x + static_cast<int>(seed_)) * 0.17) * 4.0;
        const double broad = std::cos((z - static_cast<int>(seed_)) * 0.09) * 9.0;
        const double dunes = std::sin((x + z) * 0.04) * 2.0;
        const int h = static_cast<int>(22 + rough + broad + dunes);
        return std::clamp(h, 4, kWorldHeight - 2);
    }

    void save(const std::string& filePath) {
        std::ofstream out(filePath);
        if (!out) {
            throw std::runtime_error("无法打开存档文件进行写入: " + filePath);
        }
        out << "seed " << seed_ << '\n';
        for (const auto& [coord, chunk] : chunks_) {
            for (int y = 0; y < kWorldHeight; ++y) {
                for (int z = 0; z < kChunkSize; ++z) {
                    for (int x = 0; x < kChunkSize; ++x) {
                        const auto block = chunk.get(x, y, z);
                        if (block == BlockType::Air) {
                            continue;
                        }
                        const int worldX = coord.x * kChunkSize + x;
                        const int worldZ = coord.z * kChunkSize + z;
                        out << worldX << ' ' << y << ' ' << worldZ << ' ' << static_cast<int>(block) << '\n';
                    }
                }
            }
        }
    }

    void load(const std::string& filePath) {
        std::ifstream in(filePath);
        if (!in) {
            throw std::runtime_error("无法打开存档文件进行读取: " + filePath);
        }
        chunks_.clear();
        std::string tag;
        in >> tag >> seed_;
        int x = 0;
        int y = 0;
        int z = 0;
        int rawType = 0;
        while (in >> x >> y >> z >> rawType) {
            if (y < 0 || y >= kWorldHeight) {
                continue;
            }
            setBlock(x, y, z, static_cast<BlockType>(rawType));
        }
    }

private:
    Chunk* ensureChunk(int x, int z) {
        const int chunkX = floorDiv(x, kChunkSize);
        const int chunkZ = floorDiv(z, kChunkSize);
        const Vec3i coord {chunkX, 0, chunkZ};
        auto it = chunks_.find(coord);
        if (it == chunks_.end()) {
            Chunk chunk(coord);
            generateChunk(chunk);
            it = chunks_.emplace(coord, std::move(chunk)).first;
        }
        return &it->second;
    }

    void generateChunk(Chunk& chunk) {
        const Vec3i c = chunk.coord();
        for (int localZ = 0; localZ < kChunkSize; ++localZ) {
            for (int localX = 0; localX < kChunkSize; ++localX) {
                const int worldX = c.x * kChunkSize + localX;
                const int worldZ = c.z * kChunkSize + localZ;
                const int top = terrainHeight(worldX, worldZ);
                for (int y = 0; y <= top; ++y) {
                    BlockType block = BlockType::Stone;
                    if (y == top) {
                        block = top < 18 ? BlockType::Sand : BlockType::Grass;
                    } else if (y > top - 3) {
                        block = BlockType::Dirt;
                    }
                    chunk.set(localX, y, localZ, block);
                }
                if (top < 16) {
                    for (int y = top + 1; y < 16; ++y) {
                        chunk.set(localX, y, localZ, BlockType::Water);
                    }
                }
                maybePlaceTree(chunk, localX, localZ, top);
            }
        }
    }

    void maybePlaceTree(Chunk& chunk, int localX, int localZ, int groundY) {
        if (groundY < 18 || groundY > 40) {
            return;
        }
        std::uniform_int_distribution<int> dist(0, 100);
        if (dist(noiseRng_) > 2) {
            return;
        }
        const int trunkBase = groundY + 1;
        const int trunkHeight = 3 + dist(noiseRng_) % 3;
        for (int y = trunkBase; y < trunkBase + trunkHeight && y < kWorldHeight; ++y) {
            chunk.set(localX, y, localZ, BlockType::Wood);
        }
        const int leafCenter = trunkBase + trunkHeight - 1;
        for (int y = leafCenter - 2; y <= leafCenter + 1; ++y) {
            if (y < 0 || y >= kWorldHeight) {
                continue;
            }
            for (int dz = -2; dz <= 2; ++dz) {
                for (int dx = -2; dx <= 2; ++dx) {
                    if (std::abs(dx) + std::abs(dz) > 3) {
                        continue;
                    }
                    const int tx = localX + dx;
                    const int tz = localZ + dz;
                    if (tx < 0 || tx >= kChunkSize || tz < 0 || tz >= kChunkSize) {
                        continue;
                    }
                    if (chunk.get(tx, y, tz) == BlockType::Air) {
                        chunk.set(tx, y, tz, BlockType::Leaves);
                    }
                }
            }
        }
    }

    static int floorDiv(int a, int b) {
        int q = a / b;
        int r = a % b;
        if ((r != 0) && ((r < 0) != (b < 0))) {
            --q;
        }
        return q;
    }

    static std::pair<int, int> toLocal(int x, int z) {
        int localX = x % kChunkSize;
        int localZ = z % kChunkSize;
        if (localX < 0) localX += kChunkSize;
        if (localZ < 0) localZ += kChunkSize;
        return {localX, localZ};
    }

    std::uint32_t seed_;
    std::mt19937 noiseRng_;
    std::unordered_map<Vec3i, Chunk, Vec3Hash> chunks_;
};

struct Player {
    int x = 0;
    int y = 50;
    int z = 0;
    int selectedSlot = 0;
    std::array<BlockType, 5> hotbar {BlockType::Dirt, BlockType::Stone, BlockType::Wood, BlockType::Sand, BlockType::Water};
};

class Game {
public:
    explicit Game(std::uint32_t seed) : world_(seed) {
        spawnPlayer();
    }

    void run() {
        printWelcome();
        std::string line;
        while (true) {
            std::cout << "\n> ";
            if (!std::getline(std::cin, line)) {
                break;
            }
            if (line.empty()) {
                continue;
            }
            if (!handleCommand(line)) {
                break;
            }
        }
    }

private:
    void spawnPlayer() {
        const int ground = world_.terrainHeight(0, 0);
        player_.x = 0;
        player_.z = 0;
        player_.y = ground + 2;
    }

    static void printWelcome() {
        std::cout << "=== Mini Minecraft C++ Console Edition ===\n"
                  << "命令: help, look, move dx dz, up n, down n, place x y z [block], break x y z,\n"
                  << "      render y radius, slot idx, save path, load path, quit\n";
    }

    bool handleCommand(const std::string& line) {
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "help") {
            printWelcome();
        } else if (cmd == "look") {
            printPlayerInfo();
        } else if (cmd == "move") {
            int dx = 0;
            int dz = 0;
            if (!(iss >> dx >> dz)) {
                std::cout << "用法: move dx dz\n";
            } else {
                move(dx, dz);
            }
        } else if (cmd == "up") {
            int step = 0;
            if (iss >> step) player_.y = std::min(player_.y + step, kWorldHeight - 1);
            printPlayerInfo();
        } else if (cmd == "down") {
            int step = 0;
            if (iss >> step) player_.y = std::max(player_.y - step, 1);
            printPlayerInfo();
        } else if (cmd == "place") {
            int x = 0;
            int y = 0;
            int z = 0;
            std::string name;
            if (!(iss >> x >> y >> z)) {
                std::cout << "用法: place x y z [blockName]\n";
            } else {
                iss >> name;
                place(x, y, z, name);
            }
        } else if (cmd == "break") {
            int x = 0;
            int y = 0;
            int z = 0;
            if (!(iss >> x >> y >> z)) {
                std::cout << "用法: break x y z\n";
            } else {
                world_.setBlock(x, y, z, BlockType::Air);
                std::cout << "已移除方块 @ (" << x << ", " << y << ", " << z << ")\n";
            }
        } else if (cmd == "render") {
            int y = player_.y;
            int radius = 12;
            iss >> y >> radius;
            renderSlice(y, radius);
        } else if (cmd == "slot") {
            int idx = 0;
            if (iss >> idx && idx >= 0 && idx < static_cast<int>(player_.hotbar.size())) {
                player_.selectedSlot = idx;
                std::cout << "当前物品: " << blockName(player_.hotbar[player_.selectedSlot]) << "\n";
            } else {
                std::cout << "用法: slot idx(0-4)\n";
            }
        } else if (cmd == "save") {
            std::string path;
            if (!(iss >> path)) {
                std::cout << "用法: save path\n";
            } else {
                world_.save(path);
                std::cout << "已保存到 " << path << "\n";
            }
        } else if (cmd == "load") {
            std::string path;
            if (!(iss >> path)) {
                std::cout << "用法: load path\n";
            } else {
                world_.load(path);
                std::cout << "已读取 " << path << "\n";
            }
        } else if (cmd == "quit" || cmd == "exit") {
            return false;
        } else {
            std::cout << "未知命令: " << cmd << "。输入 help 查看可用命令。\n";
        }
        return true;
    }

    void move(int dx, int dz) {
        player_.x += dx;
        player_.z += dz;
        const int targetGround = world_.terrainHeight(player_.x, player_.z);
        if (player_.y < targetGround + 1) {
            player_.y = targetGround + 1;
        }
        printPlayerInfo();
    }

    void place(int x, int y, int z, const std::string& maybeName) {
        BlockType block = player_.hotbar[player_.selectedSlot];
        if (!maybeName.empty()) {
            const auto parsed = parseBlock(maybeName);
            if (!parsed.has_value()) {
                std::cout << "未知方块类型: " << maybeName << "\n";
                return;
            }
            block = *parsed;
        }
        world_.setBlock(x, y, z, block);
        std::cout << "已放置 " << blockName(block) << " @ (" << x << ", " << y << ", " << z << ")\n";
    }

    void printPlayerInfo() {
        std::cout << "玩家位置: (" << player_.x << ", " << player_.y << ", " << player_.z << ")\n"
                  << "当前方块: " << blockName(player_.hotbar[player_.selectedSlot]) << " (slot " << player_.selectedSlot << ")\n";
        const BlockType below = world_.getBlock(player_.x, player_.y - 1, player_.z);
        std::cout << "脚下方块: " << blockName(below) << '\n';
    }

    void renderSlice(int y, int radius) {
        std::cout << "渲染高度 y=" << y << " 半径=" << radius << '\n';
        for (int z = player_.z - radius; z <= player_.z + radius; ++z) {
            for (int x = player_.x - radius; x <= player_.x + radius; ++x) {
                if (x == player_.x && z == player_.z) {
                    std::cout << '@';
                    continue;
                }
                std::cout << blockGlyph(world_.getBlock(x, y, z));
            }
            std::cout << '\n';
        }
    }

    World world_;
    Player player_;
};

}  // namespace mc

int main(int argc, char** argv) {
    std::uint32_t seed = 2024;
    if (argc > 1) {
        try {
            seed = static_cast<std::uint32_t>(std::stoul(argv[1]));
        } catch (const std::exception&) {
            std::cerr << "种子解析失败，使用默认值 2024\n";
        }
    }

    try {
        mc::Game game(seed);
        game.run();
    } catch (const std::exception& ex) {
        std::cerr << "程序异常: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
