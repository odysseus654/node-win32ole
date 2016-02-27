/*
  v8variant.cc
*/

#include "v8variant.h"
#include <node.h>
#include <nan.h>

using namespace v8;
using namespace ole32core;

namespace node_win32ole {

#define CHECK_OLE_ARGS(info, n, av0, av1) do{ \
    if(info.Length() < n) \
      return Nan::ThrowTypeError(__FUNCTION__ " takes exactly " #n " argument(s)"); \
    if(!info[0]->IsString()) \
      return Nan::ThrowTypeError(__FUNCTION__ " the first argument is not a Symbol"); \
    if(n == 1) \
      if(info.Length() >= 2) \
        if(!info[1]->IsArray()) \
          return Nan::ThrowTypeError(__FUNCTION__ " the second argument is not an Array"); \
        else av1 = info[1]; /* Array */ \
      else av1 = Nan::New<Array>(0); /* change none to Array[] */ \
    else av1 = info[1]; /* may not be Array */ \
    av0 = info[0]; \
  }while(0)

Nan::Persistent<FunctionTemplate> V8Variant::clazz;

void V8Variant::Init(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target)
{
  Nan::HandleScope scope;
  Local<FunctionTemplate> t = Nan::New<FunctionTemplate>(New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(Nan::New("V8Variant").ToLocalChecked());
  Nan::SetPrototypeMethod(t, "isA", OLEIsA);
  Nan::SetPrototypeMethod(t, "vtName", OLEVTName);
  Nan::SetPrototypeMethod(t, "toBoolean", OLEBoolean);
  Nan::SetPrototypeMethod(t, "toInt32", OLEInt32);
  Nan::SetPrototypeMethod(t, "toInt64", OLEInt64);
  Nan::SetPrototypeMethod(t, "toNumber", OLENumber);
  Nan::SetPrototypeMethod(t, "toDate", OLEDate);
  Nan::SetPrototypeMethod(t, "toUtf8", OLEUtf8);
  Nan::SetPrototypeMethod(t, "toValue", OLEValue);
//  Nan::SetPrototypeMethod(t, "New", New);
  Nan::SetPrototypeMethod(t, "call", OLECall);
  Nan::SetPrototypeMethod(t, "get", OLEGet);
  Nan::SetPrototypeMethod(t, "set", OLESet);
/*
 In ParseUnaryExpression() < v8/src/parser.cc >
 v8::Object::ToBoolean() is called directly for unary operator '!'
 instead of v8::Object::valueOf()
 so NamedPropertyHandler will not be called
 Local<Boolean> ToBoolean(); // How to fake ? override v8::Value::ToBoolean
*/
  Local<ObjectTemplate> instancetpl = t->InstanceTemplate();
  Nan::SetCallAsFunctionHandler(instancetpl, OLECallComplete);
  Nan::SetNamedPropertyHandler(instancetpl, OLEGetAttr, OLESetAttr);
//  Nan::SetIndexedPropertyHandler(instancetpl, OLEGetIdxAttr, OLESetIdxAttr);
  Nan::SetPrototypeMethod(t, "Finalize", Finalize);
  Nan::Set(target, Nan::New("V8Variant").ToLocalChecked(), t->GetFunction());
  clazz.Reset(t);
}

std::string V8Variant::CreateStdStringMBCSfromUTF8(Handle<Value> v)
{
  String::Utf8Value u8s(v);
  wchar_t * wcs = u8s2wcs(*u8s);
  if(!wcs){
    std::cerr << "[Can't allocate string (wcs)]" << std::endl;
    std::cerr.flush();
    return std::string("'!ERROR'");
  }
  char *mbs = wcs2mbs(wcs);
  if(!mbs){
    free(wcs);
    std::cerr << "[Can't allocate string (mbs)]" << std::endl;
    std::cerr.flush();
    return std::string("'!ERROR'");
  }
  std::string s(mbs);
  free(mbs);
  free(wcs);
  return s;
}

OCVariant *V8Variant::CreateOCVariant(Handle<Value> v)
{
  if (v->IsNull() || v->IsUndefined()) {
    // todo: make separate undefined type
		return new OCVariant();
  }

  BEVERIFY(done, !v.IsEmpty());
  BEVERIFY(done, !v->IsExternal());
  BEVERIFY(done, !v->IsNativeError());
  BEVERIFY(done, !v->IsFunction());
// VT_USERDEFINED VT_VARIANT VT_BYREF VT_ARRAY more...
  if(v->IsBoolean()){
    return new OCVariant(Nan::To<bool>(v).FromJust());
  }else if(v->IsArray()){
// VT_BYREF VT_ARRAY VT_SAFEARRAY
    std::cerr << "[Array (not implemented now)]" << std::endl; return NULL;
    std::cerr.flush();
  }else if(v->IsInt32()){
    return new OCVariant((long)Nan::To<int32_t>(v).FromJust());
#if(0) // may not be supported node.js / v8
  }else if(v->IsInt64()){
    return new OCVariant(Nan::To<int64_t>(v).FromJust());
#endif
  }else if(v->IsNumber()){
    std::cerr << "[Number (VT_R8 or VT_I8 bug?)]" << std::endl;
    std::cerr.flush();
// if(v->ToInteger()) =64 is failed ? double : OCVariant((longlong)VT_I8)
    return new OCVariant(Nan::To<double>(v).FromJust()); // double
  }else if(v->IsNumberObject()){
    std::cerr << "[NumberObject (VT_R8 or VT_I8 bug?)]" << std::endl;
    std::cerr.flush();
// if(v->ToInteger()) =64 is failed ? double : OCVariant((longlong)VT_I8)
    return new OCVariant(Nan::To<double>(v).FromJust()); // double
  }else if(v->IsDate()){
    double d = Nan::To<double>(v).FromJust();
    time_t sec = (time_t)(d / 1000.0);
    int msec = (int)(d - sec * 1000.0);
    struct tm *t = localtime(&sec); // *** must check locale ***
    if(!t){
      std::cerr << "[Date may not be valid]" << std::endl;
      std::cerr.flush();
      return NULL;
    }
    SYSTEMTIME syst;
    syst.wYear = t->tm_year + 1900;
    syst.wMonth = t->tm_mon + 1;
    syst.wDay = t->tm_mday;
    syst.wHour = t->tm_hour;
    syst.wMinute = t->tm_min;
    syst.wSecond = t->tm_sec;
    syst.wMilliseconds = msec;
    SystemTimeToVariantTime(&syst, &d);
    return new OCVariant(d, true); // date
  }else if(v->IsRegExp()){
    std::cerr << "[RegExp (bug?)]" << std::endl;
    std::cerr.flush();
    return new OCVariant(CreateStdStringMBCSfromUTF8(v->ToDetailString()));
  }else if(v->IsString()){
    return new OCVariant(CreateStdStringMBCSfromUTF8(v));
  }else if(v->IsStringObject()){
    std::cerr << "[StringObject (bug?)]" << std::endl;
    std::cerr.flush();
    return new OCVariant(CreateStdStringMBCSfromUTF8(v));
  }else if(v->IsObject()){
#if(0)
    std::cerr << "[Object (test)]" << std::endl;
    std::cerr.flush();
#endif
    V8Variant *v8v = V8Variant::Unwrap<V8Variant>(v->ToObject());
    if(!v8v){
      std::cerr << "[Object may not be valid (null V8Variant)]" << std::endl;
      std::cerr.flush();
      return NULL;
    }
    // std::cerr << ocv->v.vt;
    return new OCVariant(v8v->ocv);
  }else{
    std::cerr << "[unknown type (not implemented now)]" << std::endl;
    std::cerr.flush();
  }
done:
  return NULL;
}

NAN_METHOD(V8Variant::OLEIsA)
{
  DISPFUNCIN();
  V8Variant *v8v = V8Variant::Unwrap<V8Variant>(info.This());
  CHECK_V8V(v8v);
  DISPFUNCOUT();
  return info.GetReturnValue().Set(Nan::New(v8v->ocv.v.vt));
}

NAN_METHOD(V8Variant::OLEVTName)
{
  DISPFUNCIN();
  V8Variant *v8v = V8Variant::Unwrap<V8Variant>(info.This());
  CHECK_V8V(v8v);
  Local<Object> target = Nan::New(module_target);
  Array *a = Array::Cast(*(GET_PROP(target, "vt_names").ToLocalChecked()));
  DISPFUNCOUT();
  return info.GetReturnValue().Set(ARRAY_AT(a, v8v->ocv.v.vt));
}

NAN_METHOD(V8Variant::OLEBoolean)
{
  DISPFUNCIN();
  V8Variant *v8v = V8Variant::Unwrap<V8Variant>(info.This());
  CHECK_V8V(v8v);
  if(v8v->ocv.v.vt != VT_BOOL)
    return Nan::ThrowTypeError("OLEBoolean source type OCVariant is not VT_BOOL");
  bool c_boolVal = v8v->ocv.v.boolVal == VARIANT_FALSE ? 0 : !0;
  DISPFUNCOUT();
  return info.GetReturnValue().Set(c_boolVal);
}

NAN_METHOD(V8Variant::OLEInt32)
{
  DISPFUNCIN();
  V8Variant *v8v = V8Variant::Unwrap<V8Variant>(info.This());
  CHECK_V8V(v8v);
  VARIANT& v = v8v->ocv.v;
  if(v.vt != VT_I4 && v.vt != VT_INT
  && v.vt != VT_UI4 && v.vt != VT_UINT)
    return Nan::ThrowTypeError("OLEInt32 source type OCVariant is not VT_I4 nor VT_INT nor VT_UI4 nor VT_UINT");
  DISPFUNCOUT();
  return info.GetReturnValue().Set(Nan::New(v.lVal));
}

NAN_METHOD(V8Variant::OLEInt64)
{
  DISPFUNCIN();
  V8Variant *v8v = V8Variant::Unwrap<V8Variant>(info.This());
  CHECK_V8V(v8v);
  VARIANT& v = v8v->ocv.v;
  if(v.vt != VT_I8 && v.vt != VT_UI8)
    return Nan::ThrowTypeError("OLEInt64 source type OCVariant is not VT_I8 nor VT_UI8");
  DISPFUNCOUT();
  return info.GetReturnValue().Set(Nan::New<Number>(double(v.llVal)));
}

NAN_METHOD(V8Variant::OLENumber)
{
  DISPFUNCIN();
  V8Variant *v8v = V8Variant::Unwrap<V8Variant>(info.This());
  CHECK_V8V(v8v);
  VARIANT& v = v8v->ocv.v;
  if(v.vt != VT_R8)
    return Nan::ThrowTypeError("OLENumber source type OCVariant is not VT_R8");
  DISPFUNCOUT();
  return info.GetReturnValue().Set(Nan::New(v.dblVal));
}

NAN_METHOD(V8Variant::OLEDate)
{
  DISPFUNCIN();
  V8Variant *v8v = V8Variant::Unwrap<V8Variant>(info.This());
  CHECK_V8V(v8v);
  VARIANT& v = v8v->ocv.v;
  if(v.vt != VT_DATE)
    return Nan::ThrowTypeError("OLEDate source type OCVariant is not VT_DATE");
  SYSTEMTIME syst;
  VariantTimeToSystemTime(v.date, &syst);
  struct tm t = {0}; // set t.tm_isdst = 0
  t.tm_year = syst.wYear - 1900;
  t.tm_mon = syst.wMonth - 1;
  t.tm_mday = syst.wDay;
  t.tm_hour = syst.wHour;
  t.tm_min = syst.wMinute;
  t.tm_sec = syst.wSecond;
  DISPFUNCOUT();
  return info.GetReturnValue().Set(Nan::New<Date>(mktime(&t) * 1000.0 + syst.wMilliseconds).ToLocalChecked());
}

NAN_METHOD(V8Variant::OLEUtf8)
{
  DISPFUNCIN();
  V8Variant *v8v = V8Variant::Unwrap<V8Variant>(info.This());
  CHECK_V8V(v8v);
  VARIANT& v = v8v->ocv.v;
  if(v.vt != VT_BSTR)
    return Nan::ThrowTypeError("OLEUtf8 source type OCVariant is not VT_BSTR");
  Handle<Value> result;
  if(!v.bstrVal) result = Nan::Undefined(); // or Null();
  else {
    std::wstring wstr(v.bstrVal);
    char *cs = wcs2u8s(wstr.c_str());
    result = Nan::New(cs).ToLocalChecked();
    free(cs);
  }
  DISPFUNCOUT();
  return info.GetReturnValue().Set(result);
}

NAN_METHOD(V8Variant::OLEValue)
{
  OLETRACEIN();
  OLETRACEVT(info.This());
  OLETRACEFLUSH();
  Local<Object> thisObject = info.This();
  OLE_PROCESS_CARRY_OVER(thisObject);
  OLETRACEVT(thisObject);
  OLETRACEFLUSH();
  V8Variant *v8v = V8Variant::Unwrap<V8Variant>(info.This());
  if (!v8v) { std::cerr << "v8v is null"; std::cerr.flush(); }
  CHECK_V8V(v8v);
  VARIANT& v = v8v->ocv.v;
  if(v.vt == VT_EMPTY) ; // do nothing
  else if (v.vt == VT_NULL) {
    return info.GetReturnValue().SetNull();
  }
  else if (v.vt == VT_DISPATCH) {
    if (v.pdispVal == NULL) {
      return info.GetReturnValue().SetNull();
    }
    return info.GetReturnValue().Set(thisObject); // through it
  }
  else if(v.vt == VT_BOOL) OLEBoolean(info);
  else if(v.vt == VT_I4 || v.vt == VT_INT
  || v.vt == VT_UI4 || v.vt == VT_UINT) OLEInt32(info);
  else if(v.vt == VT_I8 || v.vt == VT_UI8) OLEInt64(info);
  else if(v.vt == VT_R8) OLENumber(info);
  else if(v.vt == VT_DATE) OLEDate(info);
  else if(v.vt == VT_BSTR) OLEUtf8(info);
  else if(v.vt == VT_ARRAY || v.vt == VT_SAFEARRAY){
    std::cerr << "[Array (not implemented now)]" << std::endl;
    std::cerr.flush();
  }else{
    Handle<Value> s = INSTANCE_CALL(thisObject, "vtName", 0, NULL);
    std::cerr << "[unknown type " << v.vt << ":" << *String::Utf8Value(s);
    std::cerr << " (not implemented now)]" << std::endl;
    std::cerr.flush();
  }
//done:
  OLETRACEOUT();
}

static std::string GetName(ITypeInfo *typeinfo, MEMBERID id) {
  BSTR name;
  UINT numNames = 0;
  typeinfo->GetNames(id, &name, 1, &numNames);
  if (numNames > 0) {
    return BSTR2MBCS(name);
  }
  return "";
}

/**
 * Like OLEValue, but goes one step further and reduces IDispatch objects
 * to actual JS objects. This enables things like console.log().
 **/
NAN_METHOD(V8Variant::OLEPrimitiveValue) {
  Local<Object> thisObject = info.This();
  OLE_PROCESS_CARRY_OVER(thisObject);
  OLETRACEVT(thisObject);
  OLETRACEFLUSH();
  V8Variant *v8v = V8Variant::Unwrap<V8Variant>(info.This());
  CHECK_V8V(v8v);
  VARIANT& v = v8v->ocv.v;
  if (v.vt == VT_DISPATCH) {
    IDispatch *dispatch = v.pdispVal;
    if (dispatch == NULL) {
      return info.GetReturnValue().SetNull();
    }
    Local<Object> object = Nan::New<Object>();
    ITypeInfo *typeinfo = NULL;
    HRESULT hr = dispatch->GetTypeInfo(0, LOCALE_USER_DEFAULT, &typeinfo);
    if (typeinfo) {
      TYPEATTR* typeattr;
      BASSERT(SUCCEEDED(typeinfo->GetTypeAttr(&typeattr)));
      for (int i = 0; i < typeattr->cFuncs; i++) {
        FUNCDESC *funcdesc;
        typeinfo->GetFuncDesc(i, &funcdesc);
        if (funcdesc->invkind != INVOKE_FUNC) {
          Nan::Set(object, Nan::New(GetName(typeinfo, funcdesc->memid)).ToLocalChecked(), Nan::New("Function").ToLocalChecked());
        }
        typeinfo->ReleaseFuncDesc(funcdesc);
      }
      for (int i = 0; i < typeattr->cVars; i++) {
        VARDESC *vardesc;
        typeinfo->GetVarDesc(i, &vardesc);
        Nan::Set(object, Nan::New(GetName(typeinfo, vardesc->memid)).ToLocalChecked(), Nan::New("Variable").ToLocalChecked());
        typeinfo->ReleaseVarDesc(vardesc);
      }
      typeinfo->ReleaseTypeAttr(typeattr);
    }
    return info.GetReturnValue().Set(object);
  } else {
    V8Variant::OLEValue(info);
  }
}

Handle<Object> V8Variant::CreateUndefined(void)
{
  DISPFUNCIN();
  Local<FunctionTemplate> localClazz = Nan::New(clazz);
  Local<Object> instance = Nan::NewInstance(Nan::GetFunction(localClazz).ToLocalChecked(), 0, NULL).ToLocalChecked();
  DISPFUNCOUT();
  return instance;
}

NAN_METHOD(V8Variant::New)
{
  DISPFUNCIN();
  if(!info.IsConstructCall())
    return Nan::ThrowTypeError("Use the new operator to create new V8Variant objects");
  Local<Object> thisObject = info.This();
  V8Variant *v = new V8Variant(); // must catch exception
  CHECK_V8V(v);
  v->Wrap(thisObject); // InternalField[0]
  DISPFUNCOUT();
  return info.GetReturnValue().Set(info.This());
}

Handle<Value> V8Variant::OLEFlushCarryOver(Handle<Value> v)
{
  OLETRACEIN();
  OLETRACEVT_UNDEFINED(v->ToObject());
  Handle<Value> result = Nan::Undefined();
  V8Variant *v8v = node::ObjectWrap::Unwrap<V8Variant>(v->ToObject());
  if(v8v->property_carryover.empty()){
    std::cerr << " *** carryover empty *** " << __FUNCTION__ << std::endl;
    std::cerr.flush();
    // *** or throw exception
  }else{
    const char *name = v8v->property_carryover.c_str();
    {
      OLETRACEPREARGV(Nan::New(name).ToLocalChecked());
      OLETRACEARGV();
    }
    OLETRACEFLUSH();
    Handle<Value> argv[] = {Nan::New(name).ToLocalChecked(), Nan::New<Array>(0)};
    int argc = sizeof(argv) / sizeof(argv[0]); // == 2
    v8v->property_carryover.clear();
    result = INSTANCE_CALL(v->ToObject(), "call", argc, argv);
    OCVariant *rv = V8Variant::CreateOCVariant(result);
	CHECK_OCV_UNDEFINED(rv);
    V8Variant *o = V8Variant::Unwrap<V8Variant>(v->ToObject());
	CHECK_V8V_UNDEFINED(o);
    o->ocv = *rv; // copy and don't delete rv
  }
  OLETRACEOUT();
  return result;
}

template<bool isCall>
NAN_METHOD(OLEInvoke)
{
  OLETRACEIN();
  OLETRACEVT(info.This());
  OLETRACEARGS();
  OLETRACEFLUSH();
  V8Variant *v8v = V8Variant::Unwrap<V8Variant>(info.This());
  CHECK_V8V(v8v);
  Handle<Value> av0, av1;
  CHECK_OLE_ARGS(info, 1, av0, av1);
  Array *a = Array::Cast(*av1);
  uint32_t argLen = a->Length();
  OCVariant **argchain = argLen ? (OCVariant**)alloca(sizeof(OCVariant*)*argLen) : NULL;
  for(uint32_t i = 0; i < argLen; ++i){
    OCVariant *o = V8Variant::CreateOCVariant(ARRAY_AT(a, i));
    CHECK_OCV(o);
    argchain[argLen - i - 1] = o; // why is this backwards? I'm copying the original intent(?) until I can test this
  }
  Handle<Object> vResult = V8Variant::CreateUndefined();
  String::Utf8Value u8s(av0);
  wchar_t *wcs = u8s2wcs(*u8s);
  if(!wcs && argchain) delete argchain;
  BEVERIFY(done, wcs);
  try{
    OCVariant *rv = isCall ? // argchain will be deleted automatically
      v8v->ocv.invoke(wcs, argchain, argLen, true) : v8v->ocv.getProp(wcs, argchain, argLen);
    if(rv){
      V8Variant *o = V8Variant::Unwrap<V8Variant>(vResult);
      CHECK_V8V(o);
      o->ocv = *rv; // copy and don't delete rv
    }
  }catch(OLE32coreException e){ std::cerr << e.errorMessage(*u8s); goto done;
  }catch(char *e){ std::cerr << e << *u8s << std::endl; goto done;
  }
  free(wcs); // *** it may leak when error ***
  Handle<Value> result = INSTANCE_CALL(vResult, "toValue", 0, NULL);
  OLETRACEOUT();
  return info.GetReturnValue().Set(result);
  return;
done:
  OLETRACEOUT();
  return Nan::ThrowTypeError(__FUNCTION__ " failed");
}

NAN_METHOD(V8Variant::OLECall)
{
  OLETRACEIN();
  OLETRACEVT(info.This());
  OLETRACEARGS();
  OLETRACEFLUSH();
  OLEInvoke<true>(info);
  OLETRACEOUT();
}

NAN_METHOD(V8Variant::OLEGet)
{
  OLETRACEIN();
  OLETRACEVT(info.This());
  OLETRACEARGS();
  OLETRACEFLUSH();
  OLEInvoke<false>(info);
  OLETRACEOUT();
}

NAN_METHOD(V8Variant::OLESet)
{
  OLETRACEIN();
  OLETRACEVT(info.This());
  OLETRACEARGS();
  OLETRACEFLUSH();
  Local<Object> thisObject = info.This();
  OLE_PROCESS_CARRY_OVER(thisObject);
  V8Variant *v8v = V8Variant::Unwrap<V8Variant>(info.This());
  CHECK_V8V(v8v);
  Handle<Value> av0, av1;
  CHECK_OLE_ARGS(info, 2, av0, av1);
  OCVariant *argchain = V8Variant::CreateOCVariant(av1);
  if(!argchain)
    return Nan::ThrowTypeError(__FUNCTION__ " the second argument is not valid (null OCVariant)");
  bool result = false;
  String::Utf8Value u8s(av0);
  wchar_t *wcs = u8s2wcs(*u8s);
  BEVERIFY(done, wcs);
  try{
    v8v->ocv.putProp(wcs, &argchain, 1); // argchain will be deleted automatically
  }catch(OLE32coreException e){ std::cerr << e.errorMessage(*u8s); goto done;
  }catch(char *e){ std::cerr << e << *u8s << std::endl; goto done;
  }
  free(wcs); // *** it may leak when error ***
  result = true;
  OLETRACEOUT();
  return info.GetReturnValue().Set(result);
done:
  OLETRACEOUT();
  return Nan::ThrowTypeError(__FUNCTION__ " failed");
}

NAN_METHOD(V8Variant::OLECallComplete)
{
  OLETRACEIN();
  OLETRACEVT(info.This());
  Handle<Value> result = Nan::Undefined();
  V8Variant *v8v = node::ObjectWrap::Unwrap<V8Variant>(info.This());
  if(v8v->property_carryover.empty()){
    std::cerr << " *** carryover empty *** " << __FUNCTION__ << std::endl;
    std::cerr.flush();
    // *** or throw exception
  }else{
    const char *name = v8v->property_carryover.c_str();
    {
      OLETRACEPREARGV(Nan::New(name).ToLocalChecked());
      OLETRACEARGV();
    }
    OLETRACEARGS();
    OLETRACEFLUSH();
    int argLen = info.Length();
    Handle<Array> a = Nan::New<Array>(argLen);
    for(int i = 0; i < argLen; ++i) ARRAY_SET(a, i, info[i]);
    Handle<Value> argv[] = {Nan::New(name).ToLocalChecked(), a};
    int argc = sizeof(argv) / sizeof(argv[0]); // == 2
    v8v->property_carryover.clear();
    result = INSTANCE_CALL(info.This(), "call", argc, argv);
  }
//_
//Handle<Value> r = INSTANCE_CALL(Handle<Object>::Cast(v), "toValue", 0, NULL);
  OLETRACEOUT();
  return info.GetReturnValue().Set(result);
}

NAN_PROPERTY_GETTER(V8Variant::OLEGetAttr)
{
  OLETRACEIN();
  OLETRACEVT(info.This());
  {
    OLETRACEPREARGV(property);
    OLETRACEARGV();
  }
  OLETRACEFLUSH();
  String::Utf8Value u8name(property);
  Local<Object> thisObject = info.This();

  // Why GetAttr comes twice for () in the third loop instead of CallComplete ?
  // Because of the Crankshaft v8's run-time optimizer ?
  {
    V8Variant *v8v = node::ObjectWrap::Unwrap<V8Variant>(thisObject);
    if(!v8v->property_carryover.empty()){
      if(v8v->property_carryover == *u8name){
        OLETRACEOUT();
        return info.GetReturnValue().Set(thisObject); // through it
      }
    }
  }

#if(0)
  if(std::string("call") == *u8name || std::string("get") == *u8name
  || std::string("_") == *u8name || std::string("toValue") == *u8name
//|| std::string("valueOf") == *u8name || std::string("toString") == *u8name
  ){
    OLE_PROCESS_CARRY_OVER(thisObject);
  }
#else
  if(std::string("set") != *u8name
  && std::string("toBoolean") != *u8name
  && std::string("toInt32") != *u8name && std::string("toInt64") != *u8name
  && std::string("toNumber") != *u8name && std::string("toDate") != *u8name
  && std::string("toUtf8") != *u8name
  && std::string("inspect") != *u8name && std::string("constructor") != *u8name
  && std::string("valueOf") != *u8name && std::string("toString") != *u8name
  && std::string("toLocaleString") != *u8name
  && std::string("toJSON") != *u8name
  && std::string("hasOwnProperty") != *u8name
  && std::string("isPrototypeOf") != *u8name
  && std::string("propertyIsEnumerable") != *u8name
//&& std::string("_") != *u8name
  ){
    OLE_PROCESS_CARRY_OVER(thisObject);
  }
#endif
  OLETRACEVT(thisObject);
  // Can't use INSTANCE_CALL here. (recursion itself)
  // So it returns Object's fundamental function and custom function:
  //   inspect ?, constructor valueOf toString toLocaleString
  //   hasOwnProperty isPrototypeOf propertyIsEnumerable
  static fundamental_attr fundamentals[] = {
    {0, "call", OLECall}, {0, "get", OLEGet}, {0, "set", OLESet},
    {0, "isA", OLEIsA}, {0, "vtName", OLEVTName}, // {"vt_names", ???},
    {!0, "toBoolean", OLEBoolean},
    {!0, "toInt32", OLEInt32}, {!0, "toInt64", OLEInt64},
    {!0, "toNumber", OLENumber}, {!0, "toDate", OLEDate},
    {!0, "toUtf8", OLEUtf8},
    {0, "toValue", OLEValue},
    {0, "inspect", OLEPrimitiveValue}, {0, "constructor", NULL}, {0, "valueOf", OLEPrimitiveValue},
    {0, "toString", OLEPrimitiveValue}, {0, "toLocaleString", OLEPrimitiveValue},
    {0, "toJSON", OLEPrimitiveValue},
    {0, "hasOwnProperty", NULL}, {0, "isPrototypeOf", NULL},
    {0, "propertyIsEnumerable", NULL}
  };
  for(int i = 0; i < sizeof(fundamentals) / sizeof(fundamentals[0]); ++i){
    if(std::string(fundamentals[i].name) != *u8name) continue;
    if(fundamentals[i].obsoleted){
      std::cerr << " *** ## [." << fundamentals[i].name;
      std::cerr << "()] is obsoleted. ## ***" << std::endl;
      std::cerr.flush();
    }
    OLETRACEFLUSH();
    OLETRACEOUT();
    return info.GetReturnValue().Set(Nan::GetFunction(Nan::New<FunctionTemplate>(
      fundamentals[i].func, thisObject)).ToLocalChecked());
  }
  if(std::string("_") == *u8name){ // through it when "_"
#if(0)
    std::cerr << " *** ## [._] is obsoleted. ## ***" << std::endl;
    std::cerr.flush();
#endif
  }else{
    Handle<Object> vResult = V8Variant::CreateUndefined(); // uses much memory
    OCVariant *rv = V8Variant::CreateOCVariant(thisObject);
    CHECK_OCV(rv);
    V8Variant *o = V8Variant::Unwrap<V8Variant>(vResult);
    CHECK_V8V(o);
    o->ocv = *rv; // copy and don't delete rv
    V8Variant *v8v = node::ObjectWrap::Unwrap<V8Variant>(vResult);
    v8v->property_carryover.assign(*u8name);
    OLETRACEPREARGV(property);
    OLETRACEARGV();
    OLETRACEFLUSH();
    OLETRACEOUT();
    return info.GetReturnValue().Set(vResult); // convert and hold it (uses much memory)
  }
  OLETRACEFLUSH();
  OLETRACEOUT();
  return info.GetReturnValue().Set(thisObject); // through it
}

NAN_PROPERTY_SETTER(V8Variant::OLESetAttr)
{
  OLETRACEIN();
  OLETRACEVT(info.This());
  Handle<Value> argv[] = {property, value};
  int argc = sizeof(argv) / sizeof(argv[0]);
  OLETRACEARGV();
  OLETRACEFLUSH();
  Handle<Value> r = INSTANCE_CALL(info.This(), "set", argc, argv);
  OLETRACEOUT();
  return info.GetReturnValue().Set(r);
}

NAN_METHOD(V8Variant::Finalize)
{
  DISPFUNCIN();
#if(0)
  std::cerr << __FUNCTION__ << " Finalizer is called\a" << std::endl;
  std::cerr.flush();
#endif
  V8Variant *v = node::ObjectWrap::Unwrap<V8Variant>(info.This());
  if(v) v->Finalize();
  DISPFUNCOUT();
  return info.GetReturnValue().Set(info.This());
}

void V8Variant::Finalize()
{
  if(!finalized)
  {
    ocv.Clear();
    finalized = true;
  }
}

} // namespace node_win32ole
