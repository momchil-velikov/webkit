// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 */

#include <stdio.h>
#include <math.h>
#include <assert.h>
#ifndef NDEBUG
#include <strings.h>      // for strdup
#endif

#include "array_object.h"
#include "bool_object.h"
#include "collector.h"
#include "date_object.h"
#include "debugger.h"
#include "error_object.h"
#include "function_object.h"
#include "internal.h"
#include "lexer.h"
#include "math_object.h"
#include "nodes.h"
#include "number_object.h"
#include "object.h"
#include "object_object.h"
#include "operations.h"
#include "regexp_object.h"
#include "string_object.h"

#define I18N_NOOP(s) s

extern int kjsyyparse();

using namespace KJS;

namespace KJS {
#ifdef WORDS_BIGENDIAN
  unsigned char NaN_Bytes[] = { 0x7f, 0xf8, 0, 0, 0, 0, 0, 0 };
  unsigned char Inf_Bytes[] = { 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 };
#elif defined(arm)
  unsigned char NaN_Bytes[] = { 0, 0, 0xf8, 0x7f, 0, 0, 0, 0 };
  unsigned char Inf_Bytes[] = { 0, 0, 0xf0, 0x7f, 0, 0, 0, 0 };
#else
  unsigned char NaN_Bytes[] = { 0, 0, 0, 0, 0, 0, 0xf8, 0x7f };
  unsigned char Inf_Bytes[] = { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f };
#endif

  const double NaN = *(const double*) NaN_Bytes;
  const double Inf = *(const double*) Inf_Bytes;
};

#if APPLE_CHANGES
static pthread_once_t interpreterLockOnce = PTHREAD_ONCE_INIT;
static pthread_mutex_t interpreterLock;

static void initializeInterpreterLock()
{
  pthread_mutexattr_t attr;

  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);

  pthread_mutex_init(&interpreterLock, &attr);
}

static inline void lockInterpreter()
{
  pthread_once(&interpreterLockOnce, initializeInterpreterLock);
  pthread_mutex_lock(&interpreterLock);
}

static inline void unlockInterpreter()
{
  pthread_mutex_unlock(&interpreterLock);
}

#endif


// ------------------------------ UndefinedImp ---------------------------------

UndefinedImp *UndefinedImp::staticUndefined = 0;

Value UndefinedImp::toPrimitive(ExecState */*exec*/, Type) const
{
  return Value((ValueImp*)this);
}

bool UndefinedImp::toBoolean(ExecState */*exec*/) const
{
  return false;
}

double UndefinedImp::toNumber(ExecState */*exec*/) const
{
  return NaN;
}

UString UndefinedImp::toString(ExecState */*exec*/) const
{
  return "undefined";
}

Object UndefinedImp::toObject(ExecState *exec) const
{
  Object err = Error::create(exec, TypeError, I18N_NOOP("Undefined value"));
  exec->setException(err);
  return err;
}

// ------------------------------ NullImp --------------------------------------

NullImp *NullImp::staticNull = 0;

Value NullImp::toPrimitive(ExecState */*exec*/, Type) const
{
  return Value((ValueImp*)this);
}

bool NullImp::toBoolean(ExecState */*exec*/) const
{
  return false;
}

double NullImp::toNumber(ExecState */*exec*/) const
{
  return 0.0;
}

UString NullImp::toString(ExecState */*exec*/) const
{
  return "null";
}

Object NullImp::toObject(ExecState *exec) const
{
  Object err = Error::create(exec, TypeError, I18N_NOOP("Null value"));
  exec->setException(err);
  return err;
}

// ------------------------------ BooleanImp -----------------------------------

BooleanImp* BooleanImp::staticTrue = 0;
BooleanImp* BooleanImp::staticFalse = 0;

Value BooleanImp::toPrimitive(ExecState */*exec*/, Type) const
{
  return Value((ValueImp*)this);
}

bool BooleanImp::toBoolean(ExecState */*exec*/) const
{
  return val;
}

double BooleanImp::toNumber(ExecState */*exec*/) const
{
  return val ? 1.0 : 0.0;
}

UString BooleanImp::toString(ExecState */*exec*/) const
{
  return val ? "true" : "false";
}

Object BooleanImp::toObject(ExecState *exec) const
{
  List args;
  args.append(const_cast<BooleanImp*>(this));
  return Object::dynamicCast(exec->interpreter()->builtinBoolean().construct(exec,args));
}

// ------------------------------ StringImp ------------------------------------

Value StringImp::toPrimitive(ExecState */*exec*/, Type) const
{
  return Value((ValueImp*)this);
}

bool StringImp::toBoolean(ExecState */*exec*/) const
{
  return (val.size() > 0);
}

double StringImp::toNumber(ExecState */*exec*/) const
{
  return val.toDouble();
}

UString StringImp::toString(ExecState */*exec*/) const
{
  return val;
}

Object StringImp::toObject(ExecState *exec) const
{
  List args;
  args.append(const_cast<StringImp*>(this));
  return Object::dynamicCast(exec->interpreter()->builtinString().construct(exec,args));
}

// ------------------------------ NumberImp ------------------------------------

NumberImp *NumberImp::staticNaN;

ValueImp *NumberImp::create(int i)
{
    if (SimpleNumber::fits(i))
        return SimpleNumber::make(i);
    NumberImp *imp = new NumberImp(static_cast<double>(i));
    imp->setGcAllowedFast();
    return imp;
}

ValueImp *NumberImp::create(double d)
{
    if (SimpleNumber::fits(d))
        return SimpleNumber::make((int)d);
    if (isNaN(d))
        return staticNaN;
    NumberImp *imp = new NumberImp(d);
    imp->setGcAllowedFast();
    return imp;
}

Value NumberImp::toPrimitive(ExecState *, Type) const
{
  return Number((NumberImp*)this);
}

bool NumberImp::toBoolean(ExecState *) const
{
  return !((val == 0) /* || (iVal() == N0) */ || isNaN(val));
}

double NumberImp::toNumber(ExecState *) const
{
  return val;
}

UString NumberImp::toString(ExecState *) const
{
  return UString::from(val);
}

Object NumberImp::toObject(ExecState *exec) const
{
  List args;
  args.append(const_cast<NumberImp*>(this));
  return Object::dynamicCast(exec->interpreter()->builtinNumber().construct(exec,args));
}

bool NumberImp::toUInt32(unsigned& uint32) const
{
  uint32 = (unsigned)val;
  return (double)uint32 == val;
}

// ------------------------------ LabelStack -----------------------------------

LabelStack::LabelStack(const LabelStack &other)
{
  tos = 0;
  *this = other;
}

LabelStack &LabelStack::operator=(const LabelStack &other)
{
  clear();
  tos = 0;
  StackElem *cur = 0;
  StackElem *se = other.tos;
  while (se) {
    StackElem *newPrev = new StackElem;
    newPrev->prev = 0;
    newPrev->id = se->id;
    if (cur)
      cur->prev = newPrev;
    else
      tos = newPrev;
    cur = newPrev;
    se = se->prev;
  }
  return *this;
}

bool LabelStack::push(const Identifier &id)
{
  if (id.isEmpty() || contains(id))
    return false;

  StackElem *newtos = new StackElem;
  newtos->id = id;
  newtos->prev = tos;
  tos = newtos;
  return true;
}

bool LabelStack::contains(const Identifier &id) const
{
  if (id.isEmpty())
    return true;

  for (StackElem *curr = tos; curr; curr = curr->prev)
    if (curr->id == id)
      return true;

  return false;
}

void LabelStack::pop()
{
  if (tos) {
    StackElem *prev = tos->prev;
    delete tos;
    tos = prev;
  }
}

LabelStack::~LabelStack()
{
  clear();
}

void LabelStack::clear()
{
  StackElem *prev;

  while (tos) {
    prev = tos->prev;
    delete tos;
    tos = prev;
  }
}

// ------------------------------ ContextImp -----------------------------------

// ECMA 10.2
ContextImp::ContextImp(Object &glob, ExecState *exec, Object &thisV, CodeType type,
                       ContextImp *_callingContext, FunctionImp *func, const ArgumentList *args)
    : _function(func), _arguments(args)
{
  codeType = type;
  callingCon = _callingContext;

  // create and initialize activation object (ECMA 10.1.6)
  if (type == FunctionCode || type == AnonymousCode ) {
    activation = Object(new ActivationImp(this));
    variable = activation;
  } else {
    activation = Object();
    variable = glob;
  }

  // ECMA 10.2
  switch(type) {
    case EvalCode:
      if (callingCon) {
	scope = callingCon->scopeChain().copy();
	variable = callingCon->variableObject();
	thisVal = callingCon->thisValue();
	break;
      } // else same as GlobalCode
    case GlobalCode:
      scope = List();
      scope.append(glob);
      thisVal = Object(static_cast<ObjectImp*>(glob.imp()));
      break;
    case FunctionCode:
    case AnonymousCode:
      if (type == FunctionCode) {
	scope = func->scope().copy();
	scope.prepend(activation);
      } else {
	scope = List();
	scope.append(activation);
	scope.append(glob);
      }
      variable = activation; // TODO: DontDelete ? (ECMA 10.2.3)
      thisVal = thisV;
      break;
    }

}

ContextImp::~ContextImp()
{
}

void ContextImp::pushScope(const Object &s)
{
  scope.prepend(s);
}

void ContextImp::popScope()
{
  scope.removeFirst();
}

// ------------------------------ Parser ---------------------------------------

ProgramNode *Parser::progNode = 0;
int Parser::sid = 0;

ProgramNode *Parser::parse(const UChar *code, unsigned int length, int *sourceId,
			   int *errLine, UString *errMsg)
{
  if (errLine)
    *errLine = -1;
  if (errMsg)
    *errMsg = 0;

  Lexer::curr()->setCode(code, length);
  progNode = 0;
  sid++;
  if (sourceId)
    *sourceId = sid;
  // Enable this (and the #define YYDEBUG in grammar.y) to debug a parse error
  //extern int kjsyydebug;
  //kjsyydebug=1;
  int parseError = kjsyyparse();
  ProgramNode *prog = progNode;
  progNode = 0;
  sid = -1;

  if (parseError) {
    int eline = Lexer::curr()->lineNo();
    if (errLine)
      *errLine = eline;
    if (errMsg)
      *errMsg = "Parse error at line " + UString::from(eline);
#ifndef NDEBUG
    fprintf(stderr, "KJS: JavaScript parse error at line %d.\n", eline);
#endif
    delete prog;
    return 0;
  }

  return prog;
}

// ------------------------------ InterpreterImp -------------------------------

InterpreterImp* InterpreterImp::s_hook = 0L;

void InterpreterImp::globalInit()
{
  //fprintf( stderr, "InterpreterImp::globalInit()\n" );
  UndefinedImp::staticUndefined = new UndefinedImp();
  UndefinedImp::staticUndefined->ref();
  NullImp::staticNull = new NullImp();
  NullImp::staticNull->ref();
  BooleanImp::staticTrue = new BooleanImp(true);
  BooleanImp::staticTrue->ref();
  BooleanImp::staticFalse = new BooleanImp(false);
  BooleanImp::staticFalse->ref();
  NumberImp::staticNaN = new NumberImp(NaN);
  NumberImp::staticNaN->ref();
}

void InterpreterImp::globalClear()
{
  //fprintf( stderr, "InterpreterImp::globalClear()\n" );
  UndefinedImp::staticUndefined->deref();
  UndefinedImp::staticUndefined->setGcAllowed();
  UndefinedImp::staticUndefined = 0L;
  NullImp::staticNull->deref();
  NullImp::staticNull->setGcAllowed();
  NullImp::staticNull = 0L;
  BooleanImp::staticTrue->deref();
  BooleanImp::staticTrue->setGcAllowed();
  BooleanImp::staticTrue = 0L;
  BooleanImp::staticFalse->deref();
  BooleanImp::staticFalse->setGcAllowed();
  BooleanImp::staticFalse = 0L;
  NumberImp::staticNaN->deref();
  NumberImp::staticNaN->setGcAllowed();
  NumberImp::staticNaN = 0;
}

InterpreterImp::InterpreterImp(Interpreter *interp, const Object &glob)
{
  // add this interpreter to the global chain
  // as a root set for garbage collection
#if APPLE_CHANGES
  lockInterpreter();
  m_interpreter = interp;
#endif
  if (s_hook) {
    prev = s_hook;
    next = s_hook->next;
    s_hook->next->prev = this;
    s_hook->next = this;
  } else {
    // This is the first interpreter
    s_hook = next = prev = this;
    globalInit();
  }
#if APPLE_CHANGES
  unlockInterpreter();
#endif

#if !APPLE_CHANGES
  m_interpreter = interp;
#endif
  global = glob;
  globExec = new ExecState(m_interpreter,0);
  dbg = 0;
  m_compatMode = Interpreter::NativeMode;

  // initialize properties of the global object
  initGlobalObject();

  recursion = 0;
}

void InterpreterImp::initGlobalObject()
{
  // Contructor prototype objects (Object.prototype, Array.prototype etc)

  FunctionPrototypeImp *funcProto = new FunctionPrototypeImp(globExec);
  b_FunctionPrototype = Object(funcProto);
  ObjectPrototypeImp *objProto = new ObjectPrototypeImp(globExec,funcProto);
  b_ObjectPrototype = Object(objProto);
  funcProto->setPrototype(b_ObjectPrototype);

  ArrayPrototypeImp *arrayProto = new ArrayPrototypeImp(globExec,objProto);
  b_ArrayPrototype = Object(arrayProto);
  StringPrototypeImp *stringProto = new StringPrototypeImp(globExec,objProto);
  b_StringPrototype = Object(stringProto);
  BooleanPrototypeImp *booleanProto = new BooleanPrototypeImp(globExec,objProto,funcProto);
  b_BooleanPrototype = Object(booleanProto);
  NumberPrototypeImp *numberProto = new NumberPrototypeImp(globExec,objProto,funcProto);
  b_NumberPrototype = Object(numberProto);
  DatePrototypeImp *dateProto = new DatePrototypeImp(globExec,objProto);
  b_DatePrototype = Object(dateProto);
  RegExpPrototypeImp *regexpProto = new RegExpPrototypeImp(globExec,objProto,funcProto);
  b_RegExpPrototype = Object(regexpProto);
  ErrorPrototypeImp *errorProto = new ErrorPrototypeImp(globExec,objProto,funcProto);
  b_ErrorPrototype = Object(errorProto);

  static_cast<ObjectImp*>(global.imp())->setPrototype(b_ObjectPrototype);

  // Constructors (Object, Array, etc.)
  b_Object = Object(new ObjectObjectImp(globExec, objProto, funcProto));
  b_Function = Object(new FunctionObjectImp(globExec, funcProto));
  b_Array = Object(new ArrayObjectImp(globExec, funcProto, arrayProto));
  b_String = Object(new StringObjectImp(globExec, funcProto, stringProto));
  b_Boolean = Object(new BooleanObjectImp(globExec, funcProto, booleanProto));
  b_Number = Object(new NumberObjectImp(globExec, funcProto, numberProto));
  b_Date = Object(new DateObjectImp(globExec, funcProto, dateProto));
  b_RegExp = Object(new RegExpObjectImp(globExec, funcProto, regexpProto));
  b_Error = Object(new ErrorObjectImp(globExec, funcProto, errorProto));

  // Error object prototypes
  b_evalErrorPrototype = Object(new NativeErrorPrototypeImp(globExec,errorProto,EvalError,
                                                            "EvalError","EvalError"));
  b_rangeErrorPrototype = Object(new NativeErrorPrototypeImp(globExec,errorProto,RangeError,
                                                            "RangeError","RangeError"));
  b_referenceErrorPrototype = Object(new NativeErrorPrototypeImp(globExec,errorProto,ReferenceError,
                                                            "ReferenceError","ReferenceError"));
  b_syntaxErrorPrototype = Object(new NativeErrorPrototypeImp(globExec,errorProto,SyntaxError,
                                                            "SyntaxError","SyntaxError"));
  b_typeErrorPrototype = Object(new NativeErrorPrototypeImp(globExec,errorProto,TypeError,
                                                            "TypeError","TypeError"));
  b_uriErrorPrototype = Object(new NativeErrorPrototypeImp(globExec,errorProto,URIError,
                                                            "URIError","URIError"));

  // Error objects
  b_evalError = Object(new NativeErrorImp(globExec,funcProto,b_evalErrorPrototype));
  b_rangeError = Object(new NativeErrorImp(globExec,funcProto,b_rangeErrorPrototype));
  b_referenceError = Object(new NativeErrorImp(globExec,funcProto,b_referenceErrorPrototype));
  b_syntaxError = Object(new NativeErrorImp(globExec,funcProto,b_syntaxErrorPrototype));
  b_typeError = Object(new NativeErrorImp(globExec,funcProto,b_typeErrorPrototype));
  b_uriError = Object(new NativeErrorImp(globExec,funcProto,b_uriErrorPrototype));

  // ECMA 15.3.4.1
  funcProto->put(globExec,"constructor", b_Function, DontEnum);

  global.put(globExec,"Object", b_Object, DontEnum);
  global.put(globExec,"Function", b_Function, DontEnum);
  global.put(globExec,"Array", b_Array, DontEnum);
  global.put(globExec,"Boolean", b_Boolean, DontEnum);
  global.put(globExec,"String", b_String, DontEnum);
  global.put(globExec,"Number", b_Number, DontEnum);
  global.put(globExec,"Date", b_Date, DontEnum);
  global.put(globExec,"RegExp", b_RegExp, DontEnum);
  global.put(globExec,"Error", b_Error, DontEnum);
  // Using Internal for those to have something != 0
  // (see kjs_window). Maybe DontEnum would be ok too ?
  global.put(globExec,"EvalError",b_evalError, Internal);
  global.put(globExec,"RangeError",b_rangeError, Internal);
  global.put(globExec,"ReferenceError",b_referenceError, Internal);
  global.put(globExec,"SyntaxError",b_syntaxError, Internal);
  global.put(globExec,"TypeError",b_typeError, Internal);
  global.put(globExec,"URIError",b_uriError, Internal);

  // Set the "constructor" property of all builtin constructors
  objProto->put(globExec, "constructor", b_Object, DontEnum | DontDelete | ReadOnly);
  funcProto->put(globExec, "constructor", b_Function, DontEnum | DontDelete | ReadOnly);
  arrayProto->put(globExec, "constructor", b_Array, DontEnum | DontDelete | ReadOnly);
  booleanProto->put(globExec, "constructor", b_Boolean, DontEnum | DontDelete | ReadOnly);
  stringProto->put(globExec, "constructor", b_String, DontEnum | DontDelete | ReadOnly);
  numberProto->put(globExec, "constructor", b_Number, DontEnum | DontDelete | ReadOnly);
  dateProto->put(globExec, "constructor", b_Date, DontEnum | DontDelete | ReadOnly);
  regexpProto->put(globExec, "constructor", b_RegExp, DontEnum | DontDelete | ReadOnly);
  errorProto->put(globExec, "constructor", b_Error, DontEnum | DontDelete | ReadOnly);
  b_evalErrorPrototype.put(globExec, "constructor", b_evalError, DontEnum | DontDelete | ReadOnly);
  b_rangeErrorPrototype.put(globExec, "constructor", b_rangeError, DontEnum | DontDelete | ReadOnly);
  b_referenceErrorPrototype.put(globExec, "constructor", b_referenceError, DontEnum | DontDelete | ReadOnly);
  b_syntaxErrorPrototype.put(globExec, "constructor", b_syntaxError, DontEnum | DontDelete | ReadOnly);
  b_typeErrorPrototype.put(globExec, "constructor", b_typeError, DontEnum | DontDelete | ReadOnly);
  b_uriErrorPrototype.put(globExec, "constructor", b_uriError, DontEnum | DontDelete | ReadOnly);

  // built-in values
  global.put(globExec, "NaN",        Number(NaN), DontEnum|DontDelete);
  global.put(globExec, "Infinity",   Number(Inf), DontEnum|DontDelete);
  global.put(globExec, "undefined",  Undefined(), DontEnum|DontDelete);

  // built-in functions
  global.put(globExec,"eval",       Object(new GlobalFuncImp(globExec,funcProto,GlobalFuncImp::Eval,       1)), DontEnum);
  global.put(globExec,"parseInt",   Object(new GlobalFuncImp(globExec,funcProto,GlobalFuncImp::ParseInt,   2)), DontEnum);
  global.put(globExec,"parseFloat", Object(new GlobalFuncImp(globExec,funcProto,GlobalFuncImp::ParseFloat, 1)), DontEnum);
  global.put(globExec,"isNaN",      Object(new GlobalFuncImp(globExec,funcProto,GlobalFuncImp::IsNaN,      1)), DontEnum);
  global.put(globExec,"isFinite",   Object(new GlobalFuncImp(globExec,funcProto,GlobalFuncImp::IsFinite,   1)), DontEnum);
  global.put(globExec,"escape",     Object(new GlobalFuncImp(globExec,funcProto,GlobalFuncImp::Escape,     1)), DontEnum);
  global.put(globExec,"unescape",   Object(new GlobalFuncImp(globExec,funcProto,GlobalFuncImp::UnEscape,   1)), DontEnum);

  // built-in objects
  global.put(globExec,"Math", Object(new MathObjectImp(globExec,objProto)), DontEnum);
}

InterpreterImp::~InterpreterImp()
{
  if (dbg)
    dbg->detach(m_interpreter);
  delete globExec;
  globExec = 0L;
  clear();
}

void InterpreterImp::clear()
{
  //fprintf(stderr,"InterpreterImp::clear\n");
  // remove from global chain (see init())
#if APPLE_CHANGES
  lockInterpreter();
#endif
  next->prev = prev;
  prev->next = next;
  s_hook = next;
  if (s_hook == this)
  {
    // This was the last interpreter
    s_hook = 0L;
    globalClear();
  }
#if APPLE_CHANGES
  unlockInterpreter();
#endif
}

void InterpreterImp::mark()
{
  //if (exVal && !exVal->marked())
  //  exVal->mark();
  //if (retVal && !retVal->marked())
  //  retVal->mark();
  if (UndefinedImp::staticUndefined && !UndefinedImp::staticUndefined->marked())
    UndefinedImp::staticUndefined->mark();
  if (NullImp::staticNull && !NullImp::staticNull->marked())
    NullImp::staticNull->mark();
  if (BooleanImp::staticTrue && !BooleanImp::staticTrue->marked())
    BooleanImp::staticTrue->mark();
  if (BooleanImp::staticFalse && !BooleanImp::staticFalse->marked())
    BooleanImp::staticFalse->mark();
  //fprintf( stderr, "InterpreterImp::mark this=%p global.imp()=%p\n", this, global.imp() );
  if (global.imp())
    global.imp()->mark();
  if (m_interpreter)
    m_interpreter->mark();
}

bool InterpreterImp::checkSyntax(const UString &code)
{
  // Parser::parse() returns 0 in a syntax error occurs, so we just check for that
  ProgramNode *progNode = Parser::parse(code.data(),code.size(),0,0,0);
  bool ok = (progNode != 0);
  delete progNode;
  return ok;
}

Completion InterpreterImp::evaluate(const UString &code, const Value &thisV)
{
#if APPLE_CHANGES
  lockInterpreter();
#endif
  // prevent against infinite recursion
  if (recursion >= 20) {
#if APPLE_CHANGES
    Completion result = Completion(Throw,Error::create(globExec,GeneralError,"Recursion too deep"));
    unlockInterpreter();
    return result;
#else
    return Completion(Throw,Error::create(globExec,GeneralError,"Recursion too deep"));
#endif
  }

  // parse the source code
  int sid;
  int errLine;
  UString errMsg;
  ProgramNode *progNode = Parser::parse(code.data(),code.size(),&sid,&errLine,&errMsg);

  // notify debugger that source has been parsed
  if (dbg) {
    bool cont = dbg->sourceParsed(globExec,sid,code,errLine);
    if (!cont)
#if APPLE_CHANGES
      {
	unlockInterpreter();
	return Completion(Break);
      }
#else
      return Completion(Break);
#endif
  }

  // no program node means a syntax error occurred
  if (!progNode) {
    Object err = Error::create(globExec,SyntaxError,errMsg.ascii(),errLine);
    err.put(globExec,"sid",Number(sid));
#if APPLE_CHANGES
    unlockInterpreter();
#endif
    return Completion(Throw,err);
  }

  globExec->clearException();

  recursion++;
  progNode->ref();

  Object &globalObj = globalObject();
  Object thisObj = globalObject();

  if (!thisV.isNull()) {
    // "this" must be an object... use same rules as Function.prototype.apply()
    if (thisV.isA(NullType) || thisV.isA(UndefinedType))
      thisObj = globalObject();
    else {
      thisObj = thisV.toObject(globExec);
    }
  }

  Completion res;
  if (globExec->hadException()) {
    // the thisArg.toObject() conversion above might have thrown an exception - if so,
    // propagate it back
    res = Completion(Throw,globExec->exception());
  }
  else {
    // execute the code
    ContextImp ctx(globalObj, 0, thisObj);
    ExecState newExec(m_interpreter,&ctx);
    res = progNode->execute(&newExec);
  }

  if (progNode->deref())
    delete progNode;
  recursion--;

#if APPLE_CHANGES
  unlockInterpreter();
#endif
  return res;
}

void InterpreterImp::setDebugger(Debugger *d)
{
  if (d)
    d->detach(m_interpreter);
  dbg = d;
}

// ------------------------------ InternalFunctionImp --------------------------

const ClassInfo InternalFunctionImp::info = {"Function", 0, 0, 0};

InternalFunctionImp::InternalFunctionImp(FunctionPrototypeImp *funcProto)
  : ObjectImp(funcProto)
{
}

bool InternalFunctionImp::implementsHasInstance() const
{
  return true;
}

Boolean InternalFunctionImp::hasInstance(ExecState *exec, const Value &value)
{
  if (value.type() != ObjectType)
    return Boolean(false);

  Value prot = get(exec,prototypePropertyName);
  if (prot.type() != ObjectType && prot.type() != NullType) {
    Object err = Error::create(exec, TypeError, "Invalid prototype encountered "
                               "in instanceof operation.");
    exec->setException(err);
    return Boolean(false);
  }

  Object v = Object(static_cast<ObjectImp*>(value.imp()));
  while ((v = Object::dynamicCast(v.prototype())).imp()) {
    if (v.imp() == prot.imp())
      return Boolean(true);
  }
  return Boolean(false);
}

// ------------------------------ global functions -----------------------------

double KJS::roundValue(ExecState *exec, const Value &v)
{
  if (v.type() == UndefinedType) /* TODO: see below */
    return 0.0;
  Number n = v.toNumber(exec);
  if (n.value() == 0.0)   /* TODO: -0, NaN, Inf */
    return 0.0;
  double d = floor(fabs(n.value()));
  if (n.value() < 0)
    d *= -1;

  return d;
}

#ifndef NDEBUG
#include <stdio.h>
void KJS::printInfo(ExecState *exec, const char *s, const Value &o, int lineno)
{
  if (o.isNull())
    fprintf(stderr, "KJS: %s: (null)", s);
  else {
    Value v = o;

    UString name;
    switch ( v.type() ) {
    case UnspecifiedType:
      name = "Unspecified";
      break;
    case UndefinedType:
      name = "Undefined";
      break;
    case NullType:
      name = "Null";
      break;
    case BooleanType:
      name = "Boolean";
      break;
    case StringType:
      name = "String";
      break;
    case NumberType:
      name = "Number";
      break;
    case ObjectType:
      name = Object::dynamicCast(v).className();
      if (name.isNull())
        name = "(unknown class)";
      break;
    case ListType:
      name = "List";
      break;
    default:
      break;
    }
    UString vString = v.toString(exec);
    if ( vString.size() > 50 )
      vString = vString.substr( 0, 50 ) + "...";
    // Can't use two UString::ascii() in the same fprintf call
    CString tempString( vString.cstring() );

    fprintf(stderr, "KJS: %s: %s : %s (%p)",
            s, tempString.c_str(), name.ascii(), (void*)v.imp());

    if (lineno >= 0)
      fprintf(stderr, ", line %d\n",lineno);
    else
      fprintf(stderr, "\n");
  }
}
#endif
