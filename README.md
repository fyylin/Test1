# C++ 版 Mini Minecraft（控制台完整版示例）

这是一个用 **C++20** 实现的“迷你版 Minecraft 完整流程”示例，包含：

- 区块（Chunk）管理
- 程序化地形生成（草地/沙地/石头/水/树）
- 玩家移动与热键栏
- 放置/破坏方块
- 截面渲染（ASCII）
- 世界存档与读档

> 说明：真正商业级 Minecraft 涉及渲染管线、物理、网络、多线程、资源系统、脚本系统等超大工程。这里给的是可运行、可扩展的完整核心玩法原型。

## 构建（你有 Ninja 的推荐方式）

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

## 构建（通用方式）

```bash
cmake -S . -B build
cmake --build build -j
```

## 运行

```bash
./build/mini_minecraft 12345
```

## 命令

- `help` 查看帮助
- `look` 查看玩家状态
- `move dx dz` 水平移动
- `up n` / `down n` 调整高度
- `slot idx` 切换快捷栏（0-4）
- `place x y z [block]` 放置方块（可选方块名）
- `break x y z` 破坏方块
- `render y radius` 渲染某一高度平面
- `save path` 保存世界
- `load path` 加载世界
- `quit` 退出

可用方块：`air grass dirt stone wood leaves sand water`

## 快速体验

```bash
printf 'look\nmove 3 2\nrender 20 4\nquit\n' | ./build/mini_minecraft 42
```
