set(libmport_source_files
        asset.c bundle_write.c bundle_read.c plist.c create_primative.c db.c
        dispatch.c util.c error.c
        info.c install_primative.c instance.c
        version_cmp.c check_preconditions.c delete_primative.c
        default_cbs.c merge_primative.c bundle_read_install_pkg.c
        update_primative.c bundle_read_update_pkg.c pkgmeta.c
        fetch.c index.c index_depends.c install.c clean.c setting.c
        stats.c verify.c lock.c mkdir.c hashmap.c
        mport.h mport_dispatch.h
        )
add_library(mport ${libmport_source_files})
target_link_libraries(mport PUBLIC SQLite::SQLite3 BZip2::BZip2 BlocksRuntime Dispatch)