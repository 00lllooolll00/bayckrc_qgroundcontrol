# QGroundControl 开发命令集
# 安装 just（需要 >= 1.30，home_directory() 依赖该版本）：
#   python tools/setup/install_python.py dev   （推荐；会把 rust-just 装进 .venv）
#   brew install just / cargo install just / pipx install rust-just
# 注意：Ubuntu apt 源里的 just 只有 1.21，太旧不可用。

# 以下配置项从 build-config.json 读取（读不到时回落到默认值）
qt_version := `python3 ./tools/setup/read_config.py --get qt.version 2>/dev/null || echo "6.11.1"`
cmake_min_version := `python3 ./tools/setup/read_config.py --get build.cmake_minimum_version 2>/dev/null || echo "3.25"`
gstreamer_version := `python3 ./tools/setup/read_config.py --get gstreamer.version.default 2>/dev/null || echo "1.28.4"`
qt_dir := env_var_or_default("QT_DIR", home_directory() / "Qt" / qt_version / "gcc_64")
build_type := env_var_or_default("BUILD_TYPE", "Debug")
build_dir := "build"
# 默认使用全部 CPU 核心；可用 JOBS=N 覆盖
jobs := env_var_or_default("JOBS", `python3 -c "import os; print(os.cpu_count() or 4)" 2>/dev/null || echo 4`)
export CMAKE_AUTOGEN_PARALLEL_LEVEL := env_var_or_default("CMAKE_AUTOGEN_PARALLEL_LEVEL", jobs)

# 默认命令：列出全部可用命令
default:
    @just --list --unsorted

# ─────────────────────────────────────────────────────────────────────────────
# 环境准备
# ─────────────────────────────────────────────────────────────────────────────

# 安装系统依赖（Debian/Ubuntu，需要 sudo）
deps:
    @echo "Installing dependencies (requires sudo)..."
    python3 ./tools/setup/install_dependencies --platform debian

# 初始化 git 子模块
submodules:
    git submodule update --init --recursive

# ─────────────────────────────────────────────────────────────────────────────
# 构建
# ─────────────────────────────────────────────────────────────────────────────

# 配置 CMake 构建（Debug，并打开单元测试）
configure: submodules
    python3 ./tools/configure.py -B {{build_dir}} -t {{build_type}} --testing --qt-root {{qt_dir}}

# 编译项目（对已配置的构建目录做增量编译）
build:
    cmake --build {{build_dir}} --config {{build_type}} --parallel {{jobs}}

# 配置并编译 Release 版
release:
    python3 ./tools/configure.py -B {{build_dir}} --release --qt-root {{qt_dir}}
    cmake --build {{build_dir}} --config Release --parallel {{jobs}}

# 清理构建目录（转发给 tools/clean.py，可传 --cache、--all、--dry-run）
clean *ARGS:
    ./tools/clean.py {{ARGS}}

# 清理后重新配置并编译
rebuild: clean configure build

# 完整初始化流程：依赖 → 子模块 → 配置 → 编译
setup: deps submodules configure build

# ─────────────────────────────────────────────────────────────────────────────
# 质量检查
# ─────────────────────────────────────────────────────────────────────────────

# 运行单元测试（标签过滤与 CI 一致；可用 LABELS=... EXCLUDE=... just test 覆盖）
test labels=env_var_or_default("LABELS", "Unit|Integration") exclude=env_var_or_default("EXCLUDE", "Flaky|Network"):
    cd {{build_dir}} && ctest --output-on-failure -L "{{labels}}" -LE "{{exclude}}"

# 运行 pre-commit 全套检查
lint:
    pre-commit run --all-files

# 检查代码格式（只报告问题，不修改文件）
format:
    python3 ./tools/analyze.py --tool clang-format

# 格式化代码（直接修改文件）
format-fix:
    python3 ./tools/analyze.py --tool clang-format --fix

# 运行静态分析
analyze:
    python3 ./tools/analyze.py

# 生成代码覆盖率报告
coverage:
    python3 ./tools/coverage.py

# 运行 lint 与单元测试
check: lint test

# ─────────────────────────────────────────────────────────────────────────────
# 运行与部署
# ─────────────────────────────────────────────────────────────────────────────

# 启动 QGroundControl（运行当前构建类型的产物）
run:
    ./{{build_dir}}/{{build_type}}/QGroundControl

# 构建文档
docs:
    npm run docs:build

# 用 Docker 容器构建（Ubuntu 环境，免装依赖）
docker:
    ./deploy/docker/run-docker.sh ubuntu

# ─────────────────────────────────────────────────────────────────────────────
# 实用工具
# ─────────────────────────────────────────────────────────────────────────────

# 显示当前构建配置（Qt 版本与路径、构建类型、并行任务数等）
info:
    @echo "Qt version:  {{qt_version}}"
    @echo "Qt dir:      {{qt_dir}}"
    @echo "CMake min:   {{cmake_min_version}}"
    @echo "GStreamer:   {{gstreamer_version}}"
    @echo "Build type:  {{build_type}}"
    @echo "Build dir:   {{build_dir}}"
    @echo "Jobs:        {{jobs}}"
    @echo "MOC Parallel:$CMAKE_AUTOGEN_PARALLEL_LEVEL"

# 检查各项依赖的版本是否符合要求
check-deps:
    python3 ./tools/check_deps.py

# 清理构建产物、各类缓存与生成文件
distclean:
    ./tools/clean.py --all
    rm -rf node_modules
