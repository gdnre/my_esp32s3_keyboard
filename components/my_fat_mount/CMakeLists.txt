idf_component_register(SRCS "my_fat_mount.c"
                    INCLUDE_DIRS "include"
                    REQUIRES fatfs vfs
                    )
# 需要修改下面第一个参数，要和fat分区表的label/name一样，将参数2文件夹编译为fat格式放到第一个参数的分区
# espidf的fat生成有问题，以.开头的文件名如.config，在使用readdir时无法读取到，包含空格的文件名，在readdir中读取到的文件名会丢失空格，可能是长短文件名转换的问题，总之为了防止可能问题，将文件服务器所需文件嵌入应用程序，运行时手动将其拷贝到fat中。
# 另注：如果使用spiflash作为fat存储介质，写入速度只能到60kB/s左右，谨慎频繁写入大文件
# fatfs_create_spiflash_image(ffat ./fat_data FLASH_IN_PROJECT PRESERVE_TIME)
# fatfs_create_spiflash_image(ffat ./fat_data PRESERVE_TIME)