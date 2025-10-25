#include <jni.h>
#include "PngUtilsOnLoad.hpp"

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
  return margelo::nitro::pngutils::initialize(vm);
}
