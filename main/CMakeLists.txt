set(srcs "main.c")

idf_component_register(SRCS "wifi.c" "http.c" "mesh_handler.c" "bt_provisioner.c"  "ws.c" "${srcs}"
                    INCLUDE_DIRS  ".")
