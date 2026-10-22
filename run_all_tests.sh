#!/bin/bash
# 旧版逐章 insmod 的宿主机测试已经由 QEMU 客户机里的实验取代：
# 模块按 5.15.0-173-generic 编译，宿主机内核不同就不能 insmod。
# 用法：bash run_all_tests.sh [章节]，缺省跑全部章节；章节名如 ch02、learn01、hardcode。
exec bash "$(cd "$(dirname "$0")" && pwd)/qemu_test/run_chapter.sh" "${1:-all}"
