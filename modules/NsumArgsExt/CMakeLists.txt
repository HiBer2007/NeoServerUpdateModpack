# nsum_args_ext - NSUM product-specific extension.
# CLI args (--with-editor / --use-system-git / --use-bundled-git) and the
# nsum_git_plan custom step are defined HERE (extension model), not in core.
add_library(nsum_args_ext SHARED
    src/nsum_args_ext.cpp
)
target_link_libraries(nsum_args_ext PRIVATE
    hci_core
    hci_ext
)
set_target_properties(nsum_args_ext PROPERTIES PREFIX "")