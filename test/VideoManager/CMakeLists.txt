# ============================================================================
# VideoManager Tests
# ============================================================================

# 后端关闭(如 QtMultimedia 回退)时 GStreamer 符号被裁剪，测试需一并编译出，否则链接失败。
if(QGC_ENABLE_GST_VIDEOSTREAMING)
    add_subdirectory(GStreamer)
endif()

target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        VideoManagerInitTest.cc
        VideoManagerInitTest.h
)

add_qgc_test(VideoManagerInitTest LABELS Unit)
