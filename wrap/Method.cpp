/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation, 
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file Method.ccp
 * @author Frank Dellaert
 * @author Richard Roberts
 **/

#include "Method.h"
#include "Class.h"
#include "utilities.h"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include <iostream>
#include <fstream>

using namespace std;
using namespace wrap;

/* ************************************************************************* */
/// Cython: Rename functions which names are python keywords
static const std::array<std::string,2> pythonKeywords{{"print", "lambda"}};
static std::string pyRename(const std::string& name) {
    if (std::find(pythonKeywords.begin(), pythonKeywords.end(), name) ==
        pythonKeywords.end())
        return name;
    else
        return name + "_";
}

/* ************************************************************************* */
bool Method::addOverload(Str name, const ArgumentList& args,
    const ReturnValue& retVal, bool is_const,
    boost::optional<const Qualified> instName, bool verbose) {
  bool first = MethodBase::addOverload(name, args, retVal, instName, verbose);
  if (first)
    is_const_ = is_const;
  else if (is_const && !is_const_)
    throw std::runtime_error(
        "Method::addOverload now designated as const whereas before it was not");
  else if (!is_const && is_const_)
    throw std::runtime_error(
        "Method::addOverload now designated as non-const whereas before it was");
  return first;
}

/* ************************************************************************* */
void Method::proxy_header(FileWriter& proxyFile) const {
  proxyFile.oss << "    function varargout = " << matlabName()
      << "(this, varargin)\n";
}

/* ************************************************************************* */
string Method::wrapper_call(FileWriter& wrapperFile, Str cppClassName,
    Str matlabUniqueName, const ArgumentList& args) const {
  // check arguments
  // extra argument obj -> nargin-1 is passed !
  // example: checkArguments("equals",nargout,nargin-1,2);
  wrapperFile.oss << "  checkArguments(\"" << matlabName()
      << "\",nargout,nargin-1," << args.size() << ");\n";

  // get class pointer
  // example: shared_ptr<Test> = unwrap_shared_ptr< Test >(in[0], "Test");
  wrapperFile.oss << "  Shared obj = unwrap_shared_ptr<" << cppClassName
      << ">(in[0], \"ptr_" << matlabUniqueName << "\");" << endl;

  // unwrap arguments, see Argument.cpp, we start at 1 as first is obj
  args.matlab_unwrap(wrapperFile, 1);

  // call method and wrap result
  // example: out[0]=wrap<bool>(obj->return_field(t));
  string expanded = "obj->" + name_;
  if (templateArgValue_)
    expanded += ("<" + templateArgValue_->qualifiedName("::") + ">");

  return expanded;
}

/* ************************************************************************* */
void Method::emit_cython_pxd(FileWriter& file, const Class& cls) const {
  for(size_t i = 0; i < nrOverloads(); ++i) {
    file.oss << "\t\t";
    returnVals_[i].emit_cython_pxd(file, cls.cythonClass());
    file.oss << pyRename(name_) + " \"" + name_ + "\"" << "(";
    argumentList(i).emit_cython_pxd(file, cls.cythonClass());
    file.oss << ")";
    if (is_const_) file.oss << " const";
    file.oss << "\n";
  }
}

/* ************************************************************************* */
void Method::emit_cython_pyx(FileWriter& file, const Class& cls) const {
  string funcName = pyRename(name_);
  if (funcName == "print_")
    file.oss << "\tdef __str__(self):\n\t\tself.print_('')\n\t\treturn ''\n";
  size_t N = nrOverloads();
  bool hasPrint = false;
  for(size_t i = 0; i < N; ++i) {
    // Function definition
    file.oss << "\tdef " << funcName;
    if (funcName == "print_") hasPrint = true;
    // modify name of function instantiation as python doesn't allow overloads
    // e.g. template<T={A,B,C}> funcName(...) --> funcNameA, funcNameB, funcNameC
    // TODO: handle overloading properly!! This is lazy...
    if (templateArgValue_) file.oss << templateArgValue_->name();
    // change function overload's name: funcName(...) --> funcName_1, funcName_2
    // TODO: handle overloading properly!! This is lazy...
    file.oss <<  ((i>0)? "_" + to_string(i):"");
    // funtion arguments
    file.oss << "(self";
    if (argumentList(i).size() > 0) file.oss << ", ";
    argumentList(i).emit_cython_pyx(file);
    file.oss << "):\n";

    /// Call cython corresponding function and return
    string caller = "self." + cls.pyxCythonObj() + ".get()";
    emit_cython_pyx_function_call(file, "\t\t", caller, funcName, i, cls);
  }
}
/* ************************************************************************* */
