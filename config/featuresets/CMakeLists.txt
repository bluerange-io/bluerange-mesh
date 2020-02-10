add_all_files_to_targets("${SIMULATOR_TARGETS}")
target_include_directories_multi("${NATIVE_TARGETS}" .)
target_include_directories_multi("${ALL_TARGETS}" "./fragments")
