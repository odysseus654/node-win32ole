#ifndef __V8VARIANT_H__
#define __V8VARIANT_H__

#include <node.h>
#include <nan.h>
#include "node_win32ole.h"
#include "ole32core.h"

namespace node_win32ole {

typedef struct _fundamental_attr {
  bool obsoleted;
  const char *name;
  NAN_METHOD((*func));
} fundamental_attr;

class V8Variant : public node::ObjectWrap {
public:
  static Nan::Persistent<FunctionTemplate> clazz;
  static void Init(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target);
  static std::string CreateStdStringMBCSfromUTF8(Handle<Value> v); // *** p.
  static ole32core::OCVariant *CreateOCVariant(Handle<Value> v); // *** private
  static NAN_METHOD(OLEIsA);
  static NAN_METHOD(OLEVTName);
  static NAN_METHOD(OLEBoolean); // *** p.
  static NAN_METHOD(OLEInt32); // *** p.
  static NAN_METHOD(OLEInt64); // *** p.
  static NAN_METHOD(OLENumber); // *** p.
  static NAN_METHOD(OLEDate); // *** p.
  static NAN_METHOD(OLEUtf8); // *** p.
  static NAN_METHOD(OLEValue);
  static NAN_METHOD(OLEPrimitiveValue);
  static Handle<Object> CreateUndefined(void); // *** private
  static NAN_METHOD(New);
  static Handle<Value> OLEFlushCarryOver(Handle<Value> v); // *** p.
  static NAN_METHOD(OLECall);
  static NAN_METHOD(OLEGet);
  static NAN_METHOD(OLESet);
  static NAN_METHOD(OLECallComplete); // *** p.
  static NAN_PROPERTY_GETTER(OLEGetAttr);
  static NAN_PROPERTY_SETTER(OLESetAttr);
  static NAN_METHOD(Finalize);
public:
  V8Variant() : finalized(false), property_carryover() {}
  ~V8Variant() { if(!finalized) Finalize(); }
  ole32core::OCVariant ocv;
protected:
  static void Dispose(const Nan::WeakCallbackInfo<ole32core::OCVariant> &data);
  void Finalize();
protected:
  bool finalized;
  std::string property_carryover;
};

} // namespace node_win32ole

#endif // __V8VARIANT_H__
