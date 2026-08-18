#include <jni.h>
#include <string>
#include <vector>
#include <iostream>
#include <pthread.h>
#include <android/native_activity.h>

extern "C" {
    extern int main();
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_alu_AluBridge_main(JNIEnv *env, jobject thiz) {
    int result = main();
    return result;
}

extern "C" {
    static JavaVM* g_vm = nullptr;
    static jobject g_activity = nullptr;
    static void* alu_thread_func(void*) {
        main();
        return nullptr;
    }
    JNIEXPORT void JNICALL ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize) {
        g_vm = activity->vm;
        JNIEnv* env;
        activity->vm->GetEnv((void**)&env, JNI_VERSION_1_6);
        g_activity = env->NewGlobalRef(activity->clazz);
        pthread_t thread;
        pthread_create(&thread, nullptr, alu_thread_func, nullptr);
    }
    int alu_os_get_screen_width() {
        if (!g_vm || !g_activity) return 0;
        JNIEnv* env; g_vm->AttachCurrentThread(&env, nullptr);
        jclass activityClass = env->GetObjectClass(g_activity);
        jmethodID getResources = env->GetMethodID(activityClass, "getResources", "()Landroid/content/res/Resources;");
        jobject resources = env->CallObjectMethod(g_activity, getResources);
        jclass resourcesClass = env->GetObjectClass(resources);
        jmethodID getDisplayMetrics = env->GetMethodID(resourcesClass, "getDisplayMetrics", "()Landroid/util/DisplayMetrics;");
        jobject metrics = env->CallObjectMethod(resources, getDisplayMetrics);
        jclass metricsClass = env->GetObjectClass(metrics);
        jfieldID widthPixels = env->GetFieldID(metricsClass, "widthPixels", "I");
        int width = env->GetIntField(metrics, widthPixels);
        g_vm->DetachCurrentThread();
        return width;
    }
    void alu_os_show_toast(char* msg) {
        if (!g_vm || !g_activity) return;
        JNIEnv* env; g_vm->AttachCurrentThread(&env, nullptr);
        jclass toastClass = env->FindClass("android/widget/Toast");
        jmethodID makeText = env->GetStaticMethodID(toastClass, "makeText", "(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;");
        jmethodID show = env->GetMethodID(toastClass, "show", "()V");
        jstring jmsg = env->NewStringUTF(msg);
        jobject toast = env->CallStaticObjectMethod(toastClass, makeText, g_activity, jmsg, 0);
        env->CallVoidMethod(toast, show);
        env->DeleteLocalRef(jmsg);
        g_vm->DetachCurrentThread();
    }
}

