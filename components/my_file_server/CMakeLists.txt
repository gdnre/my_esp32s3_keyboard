idf_component_register(SRCS "my_fs_private.c" "my_file_server.c"
                    INCLUDE_DIRS "include"
                    REQUIRES my_fat_mount my_http_start my_config json
                    EMBED_FILES favicon.ico index.html script.js style.css control.html control.js control.css)
