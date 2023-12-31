//
// Created by chic on 2023/11/26.
//

#include <cstdio>
#include <locale>
#include <linux/ashmem.h>
#include "linker.h"

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <vector>
#include "elf_symbol_resolver.h"
#include "linker_debug.h"
#include <sys/syscall.h>
#include <sys/utsname.h>


android_namespace_t* g_default_namespace = static_cast<android_namespace_t *>(resolve_elf_internal_symbol(get_android_linker_path(), "__dl_g_default_namespace"));

soinfo* (*soinf_alloc_fun)(android_namespace_t* , const char* ,const struct stat* , off64_t ,uint32_t ) = (soinfo* (*)(android_namespace_t* , const char* ,const struct stat* , off64_t ,uint32_t )) resolve_elf_internal_symbol(get_android_linker_path(),"__dl__Z12soinfo_allocP19android_namespace_tPKcPK4statlj");

soinfo* (*solist_get_head)() = (soinfo* (*)())resolve_elf_internal_symbol(get_android_linker_path(), "__dl__Z15solist_get_headv");

soinfo* (*solist_get_somain)() = (soinfo* (*)())resolve_elf_internal_symbol(get_android_linker_path(), "__dl__Z17solist_get_somainv");

char* (*soinfo_get_soname)(soinfo*) = (char* (*)(soinfo*))resolve_elf_internal_symbol(get_android_linker_path(), "__dl__ZNK6soinfo10get_sonameEv");



static inline uintptr_t untag_address(uintptr_t p) {
#if defined(__aarch64__)
    return p & ((1ULL << 56) - 1);
#else
    return p;
#endif
}

template <typename T>
static inline T* untag_address(T* p) {
    return reinterpret_cast<T*>(untag_address(reinterpret_cast<uintptr_t>(p)));
}
soinfo* find_system_library_byname(const char* soname) {

    for (soinfo* si = solist_get_head(); si != nullptr; si = si->next) {
        char* ret_name = soinfo_get_soname(si);
        if(ret_name!= nullptr){
            LOGE("get_soname : %s",ret_name);
            if(0 == strncmp(ret_name,soname, strlen(soname))) {
                return si;
            }
        }
        else{
            LOGE("get_soname == null so->realpath: %s",si->get_realpath());
        }

    }
    return nullptr;
}

soinfo* find_containing_library(const void* p) {

//    static soinfo* (*solist_get_head)() = NULL;
//    if (!solist_get_head)
//        solist_get_head = (soinfo* (*)())resolve_elf_internal_symbol(get_android_linker_path(), "__dl__Z15solist_get_headv");

//    static soinfo* (*solist_get_somain)() = NULL;
//    if (!solist_get_somain)
//        solist_get_somain = (soinfo* (*)())resolve_elf_internal_symbol(get_android_linker_path(), "__dl__Z17solist_get_somainv");

    ElfW(Addr) address = reinterpret_cast<ElfW(Addr)>(untag_address(p));
    for (soinfo* si = solist_get_head(); si != nullptr; si = si->next) {
        if (address < si->base || address - si->base >= si->size) {
            continue;
        }
        ElfW(Addr) vaddr = address - si->load_bias;
        for (size_t i = 0; i != si->phnum; ++i) {
            const ElfW(Phdr)* phdr = &si->phdr[i];
            if (phdr->p_type != PT_LOAD) {
                continue;
            }
            if (vaddr >= phdr->p_vaddr && vaddr < phdr->p_vaddr + phdr->p_memsz) {
                return si;
            }
        }
    }
    return nullptr;
}

void linker_protect(){


    void* g_soinfo_allocator = static_cast<void *>(resolve_elf_internal_symbol(get_android_linker_path(), "__dl__ZL18g_soinfo_allocator"));
    void* g_soinfo_links_allocator = static_cast<void *>(resolve_elf_internal_symbol(get_android_linker_path(), "__dl__ZL24g_soinfo_links_allocator"));
    void* g_namespace_allocator = static_cast<void *>(resolve_elf_internal_symbol(get_android_linker_path(), "__dl__ZL21g_namespace_allocator"));
    void* g_namespace_list_allocator = static_cast<void *>(resolve_elf_internal_symbol(get_android_linker_path(), "__dl__ZL26g_namespace_list_allocator"));


    void (*protect_all)(void*,int prot) = (void (*)(void*,int prot))resolve_elf_internal_symbol(get_android_linker_path(),"__dl__ZN20LinkerBlockAllocator11protect_allEi");
    protect_all(g_soinfo_allocator,PROT_READ | PROT_WRITE);      //arg1 = 0x73480D23D8
    protect_all(g_soinfo_links_allocator,PROT_READ | PROT_WRITE);
    protect_all(g_namespace_allocator,PROT_READ | PROT_WRITE);
    protect_all(g_namespace_list_allocator,PROT_READ | PROT_WRITE);
}

void linker_unprotect(){


    void* g_soinfo_allocator = static_cast<void *>(resolve_elf_internal_symbol(get_android_linker_path(), "__dl__ZL18g_soinfo_allocator"));
    void* g_soinfo_links_allocator = static_cast<void *>(resolve_elf_internal_symbol(get_android_linker_path(), "__dl__ZL24g_soinfo_links_allocator"));
    void* g_namespace_allocator = static_cast<void *>(resolve_elf_internal_symbol(get_android_linker_path(), "__dl__ZL21g_namespace_allocator"));
    void* g_namespace_list_allocator = static_cast<void *>(resolve_elf_internal_symbol(get_android_linker_path(), "__dl__ZL26g_namespace_list_allocator"));


    void (*protect_all)(void*,int prot) = (void (*)(void*,int prot))resolve_elf_internal_symbol(get_android_linker_path(),"__dl__ZN20LinkerBlockAllocator11protect_allEi");
    protect_all(g_soinfo_allocator,PROT_READ  );
    protect_all(g_soinfo_links_allocator,PROT_READ  );
    protect_all(g_namespace_allocator,PROT_READ  );
    protect_all(g_namespace_list_allocator,PROT_READ  );
}


soinfo* soinfo_alloc(ApkNativeInfo &apkNativeInfo){


    soinfo* (*soinf_alloc_fun)(android_namespace_t* , const char* ,const struct stat* , off64_t ,uint32_t ) = (soinfo* (*)(android_namespace_t* , const char* ,const struct stat* , off64_t ,uint32_t )) resolve_elf_internal_symbol(get_android_linker_path(),"__dl__Z12soinfo_allocP19android_namespace_tPKcPK4statlj");
    soinfo* si = soinf_alloc_fun(g_default_namespace, apkNativeInfo.libname.c_str(), nullptr, 0, RTLD_GLOBAL);
    return si;
}


//void* LoadNativeSoByMem(ApkNativeInfo &apkNativeInfo){
//
//
//
//
//
//
//
////    LoadTask task = new LoadTask();
//
//
//    soinfo * si_ = soinfo_alloc(apkNativeInfo);
//    if (si_ == nullptr) {
//        return nullptr;
//    }
//
////    task->set_soinfo(si);
//
//
//
//    ElfReader * elf_reader = new ElfReader();
//    apkNativeInfo.libname = apkNativeInfo.file_stat.m_filename;
//
//    if(!elf_reader->Read(apkNativeInfo.libname.c_str(),apkNativeInfo.fd,0,apkNativeInfo.file_stat.m_uncomp_size)){
//        LOGE("elf_reader Read failed");
//        return nullptr;
//    }
//
//
//
//    address_space_params  default_params;
//    elf_reader->Load(&default_params);
//
//
//
//    si_->base = elf_reader->load_start();
//    si_->size = elf_reader->load_size();
//    si_->set_mapped_by_caller(elf_reader->is_mapped_by_caller());
//    si_->load_bias = elf_reader->load_bias();
//    si_->phnum = elf_reader->phdr_count();
//    si_->phdr = elf_reader->loaded_phdr();
//
//    si_->prelink_image();
//    si_->set_dt_flags_1(si_->get_dt_flags_1() | DF_1_GLOBAL);
//    si_->call_constructors();
//
//    LOGE("%s","successful");
//}


int memfd_create(const char* name, unsigned int flags) {
    // Check kernel version supports memfd_create(). Some older kernels segfault executing
    // memfd_create() rather than returning ENOSYS (b/116769556).
    static constexpr int kRequiredMajor = 3;
    static constexpr int kRequiredMinor = 17;
    struct utsname uts;
    int major, minor;
    if (uname(&uts) != 0 ||
        strcmp(uts.sysname, "Linux") != 0 ||
        sscanf(uts.release, "%d.%d", &major, &minor) != 2 ||
        (major < kRequiredMajor || (major == kRequiredMajor && minor < kRequiredMinor))) {
        errno = ENOSYS;
        return -1;
    }
    return syscall(__NR_memfd_create, name, flags);
}




uint8_t * Creatememfd(int *fd, int size){

    *fd = memfd_create("test", MFD_CLOEXEC);
    if (*fd < 0) {
        perror("Creatememfd Filede: open /dev/ashmem failed");
        exit(EXIT_FAILURE);
    }

    ftruncate(*fd, size);

    uint8_t *ptr = static_cast<uint8_t *>(mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, *fd,0));
    if (ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    return ptr;
}




soinfo* find_library(std::vector<LoadTask*> &load_tasks,const char *soname) {

    LoadTask* find_soinfo = nullptr;
    for (auto&& task : load_tasks) {
        if(0 == strncmp(task->get_soinfo()->get_soname(),soname, strlen(soname))){
            find_soinfo = task;
        }
    }
    if(find_soinfo != nullptr) {
        if(find_soinfo->get_soinfo()->is_linked()){
            find_soinfo->soload(load_tasks);
        }
        return find_soinfo->get_soinfo();

    }else{
        return find_system_library_byname(soname);
    }

}

const char* fix_dt_needed(const char* dt_needed, const char* sopath __unused) {
#if !defined(__LP64__)
    int app_target_api_level = android_get_application_target_sdk_version();
    if (app_target_api_level < 23) {
        const char* bname = basename(dt_needed);
        if (bname != dt_needed) {
            DL_WARN_documented_change(23,
                                      "invalid-dt_needed-entries-enforced-for-api-level-23",
                                      "library \"%s\" has invalid DT_NEEDED entry \"%s\"",
                                      sopath, dt_needed, app_target_api_level);
//      add_dlwarning(sopath, "invalid DT_NEEDED entry",  dt_needed);
        }

        return bname;
    }
#endif
    return dt_needed;
}


template<typename F>
static void for_each_dt_needed(const ElfReader& elf_reader, F action) {
    for (const ElfW(Dyn)* d = elf_reader.dynamic(); d->d_tag != DT_NULL; ++d) {
        if (d->d_tag == DT_NEEDED) {
            action(fix_dt_needed(elf_reader.get_string(d->d_un.d_val), elf_reader.name()));
        }
    }
}


void LoadTask::soload(std::vector<LoadTask*> &load_tasks) {

    SymbolLookupList lookup_list;

    for_each_dt_needed(get_elf_reader(), [&](const char* name) {
        LOGE("NEED name: %s",name);
        soinfo* si = find_library(load_tasks, name);
        SymbolLookupLib SyLib = si->get_lookup_lib();
        lookup_list.addSymbolLib(SyLib);
    });

    address_space_params  default_params;
    load(&default_params);
    get_soinfo()->prelink_image();
    get_soinfo()->set_dt_flags_1(get_soinfo()->get_dt_flags_1() | DF_1_GLOBAL);
    get_soinfo()->link_image(lookup_list);


}

void LoadTask::init_call() {

    get_soinfo()->set_linked();
    get_soinfo()->call_constructors();
}

//void load_dex(JNIEnv *pEnv) {
//    jobjectArray JAAR = nullptr;
//    auto classloader = pEnv->FindClass("java/lang/ClassLoader");
//    auto getsyscl_mid = pEnv->GetStaticMethodID(classloader, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
//    auto sys_classloader = pEnv->CallStaticObjectMethod(classloader, getsyscl_mid);
//    jmethodID method_loadClass = pEnv->GetMethodID(classloader,"loadClass","(Ljava/lang/String;)Ljava/lang/Class;");
//
//    if (!sys_classloader){
//        LOGE("getSystemClassLoader failed!!!");
//        return;
//    }
//    auto in_memory_classloader = pEnv->FindClass( "dalvik/system/InMemoryDexClassLoader");
//    auto initMid = pEnv->GetMethodID( in_memory_classloader, "<init>",
//                                      "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
//    auto byte_buffer_class = pEnv->FindClass("java/nio/ByteBuffer");
//    auto dex_buffer = pEnv->NewDirectByteBuffer(dex_addr, dex_size);
//    if (auto my_cl = pEnv->NewObject( in_memory_classloader, initMid, dex_buffer, sys_classloader)) {
//        jobject  sand_class_loader_ = pEnv->NewGlobalRef( my_cl);
//
//        jstring xposed = pEnv->NewStringUTF("com.rxposed.sandhooktextapp.XposedTest");
//        jobject XposedTest = pEnv->CallObjectMethod(sand_class_loader_,method_loadClass,xposed);
//
//        jclass Class_cls = pEnv->FindClass("java/lang/Class");
//        jmethodID clsmethod_method = pEnv->GetMethodID(Class_cls,"getMethod","(Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;");
//        jclass Method_cls = pEnv->FindClass("java/lang/reflect/Method");
//        jmethodID invoke_met =  pEnv->GetMethodID(Method_cls,"invoke","(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;");
//
//        jobject user_metohd = nullptr;
//        jstring  call_method_name = pEnv->NewStringUTF("native_hook");
//        user_metohd = pEnv->CallObjectMethod(XposedTest, clsmethod_method,call_method_name,JAAR);
//        pEnv->CallObjectMethod(user_metohd, invoke_met,XposedTest,JAAR);
//
//    } else {
//        LOGE("InMemoryDexClassLoader creation failed!!!");
//        return;
//    }
//}



bool LoadApkModule(JNIEnv *pEnv,char * apkSource){



    jobjectArray JAAR = nullptr;
    jobject currentDexLoad = nullptr;
    auto classloader = pEnv->FindClass("java/lang/ClassLoader");
    auto getsyscl_mid = pEnv->GetStaticMethodID(classloader, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    auto sys_classloader = pEnv->CallStaticObjectMethod(classloader, getsyscl_mid);
    jmethodID method_loadClass = pEnv->GetMethodID(classloader,"loadClass","(Ljava/lang/String;)Ljava/lang/Class;");

    if (!sys_classloader){
        LOGE("getSystemClassLoader failed!!!");
        return false;
    }
    auto in_memory_classloader = pEnv->FindClass( "dalvik/system/InMemoryDexClassLoader");
    auto initMid = pEnv->GetMethodID( in_memory_classloader, "<init>","(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
    auto byte_buffer_class = pEnv->FindClass("java/nio/ByteBuffer");


    std::unordered_map<const soinfo*, ElfReader> readers_map;
    std::vector<LoadTask*> load_tasks;
    std::vector<uint8_t*> load_dexs;
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    mz_bool status = mz_zip_reader_init_file(&zip_archive, apkSource, 0);
    if (!status) {
        printf("Could not initialize zip reader.\n");
        return false;
    }
    std::vector<ApkNativeInfo> vec_apkNativeInfo;
    int file_count = (int)mz_zip_reader_get_num_files(&zip_archive);
    for (int i = 0; i < file_count; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
            printf("Could not retrieve file info.\n");
            mz_zip_reader_end(&zip_archive);
            return false;
        }
        if(strstr(file_stat.m_filename,APK_NATIVE_LIB)!= NULL) {
            int fd;
            uint8_t * somem_addr = Creatememfd(&fd, file_stat.m_uncomp_size);

            if (!somem_addr) {
                printf("Failed to allocate memory.\n");
                mz_zip_reader_end(&zip_archive);
                return false;
            }
            if (!mz_zip_reader_extract_to_mem(&zip_archive, i, somem_addr, file_stat.m_uncomp_size, 0)) {
                printf("Failed to extract file.\n");
                mz_zip_reader_end(&zip_archive);
                return false;
            }

            LoadTask* task =  LoadTask::create(file_stat.m_filename, nullptr,g_default_namespace, &readers_map);
            task->set_fd(fd, false);
            task->set_file_offset(0);
            task->set_file_size(file_stat.m_uncomp_size);
            load_tasks.push_back(task);
//            printf("Filename: \"%s\", Comment: \"%s\", Uncompressed size: %llu\n",file_stat.m_filename, file_stat.m_comment ? file_stat.m_comment : "",(mz_uint64) file_stat.m_uncomp_size);
        }
        if(strstr(file_stat.m_filename, ".dex") != NULL){
            uint8_t *file_data = (unsigned char *)malloc(file_stat.m_uncomp_size);
            if (!file_data) {
                printf("Memory allocation failed\n");
                mz_zip_reader_end(&zip_archive);
                return false;
            }
            if (!mz_zip_reader_extract_to_mem(&zip_archive, i, file_data, file_stat.m_uncomp_size, 0)) {
                printf("Failed to extract file\n");
                free(file_data);
                mz_zip_reader_end(&zip_archive);
                return false;
            }
            auto dex_buffer = pEnv->NewDirectByteBuffer(file_data, file_stat.m_uncomp_size);
            if (currentDexLoad == nullptr ) {
                currentDexLoad = pEnv->NewObject( in_memory_classloader, initMid, dex_buffer, sys_classloader);
            } else {
                currentDexLoad = pEnv->NewObject( in_memory_classloader, initMid, dex_buffer, currentDexLoad);
            }

        }
    }
    mz_zip_reader_end(&zip_archive);


    linker_protect();

    for (size_t i = 0; i<load_tasks.size(); ++i) {

        LoadTask* task = load_tasks[i];
        soinfo* si = soinf_alloc_fun(g_default_namespace, ""/*real path*/, nullptr, 0, RTLD_GLOBAL);
        if (si == nullptr) {
            return false;
        }
        task->set_soinfo(si);

        if (!task->read()) {
//            soinfo_free(si);
            task->set_soinfo(nullptr);
            return false;
        }
        const ElfReader& elf_reader = task->get_elf_reader();
        for (const ElfW(Dyn)* d = elf_reader.dynamic(); d->d_tag != DT_NULL; ++d) {
            if (d->d_tag == DT_RUNPATH) {
                si->set_dt_runpath(elf_reader.get_string(d->d_un.d_val));
            }
            if (d->d_tag == DT_SONAME) {
                si->set_soname(elf_reader.get_string(d->d_un.d_val));
            }
        }
    }


    for (auto&& task : load_tasks) {
        task->soload(load_tasks);
    }

    linker_unprotect();


    if (currentDexLoad != nullptr ) {
        jobject  sand_class_loader_ = pEnv->NewGlobalRef( currentDexLoad);

        jstring LoadEntry_cls = pEnv->NewStringUTF("com.hepta.fridaload.LoadEntry");
        jobject LoadEntrycls_obj = pEnv->CallObjectMethod(sand_class_loader_,method_loadClass,LoadEntry_cls);
        jmethodID call_method_mth = pEnv->GetStaticMethodID(static_cast<jclass>(LoadEntrycls_obj), "text", "(Ljava/lang/String;)V");
        jstring aerg = pEnv->NewStringUTF("test load hiedapk");
        pEnv->CallStaticVoidMethod(static_cast<jclass>(LoadEntrycls_obj), call_method_mth,aerg);
    }

    return true;

}






















































