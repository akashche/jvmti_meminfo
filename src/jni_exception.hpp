/* 
 * File:   jni_exception.hpp
 * Author: alex
 *
 * Created on April 22, 2017, 11:55 AM
 */

#ifndef MEMINFO_JNI_EXCEPTION_HPP
#define	MEMINFO_JNI_EXCEPTION_HPP

#include "staticlib/support/exception.hpp"

namespace jni_helper {

/**
 * Module specific exception
 */
class jni_exception : public sl::support::exception {
public:
    /**
     * Default constructor
     */
    jni_exception() = default;

    /**
     * Constructor with message
     * 
     * @param msg error message
     */
    jni_exception(const std::string& msg) :
    sl::support::exception(msg) { }

};

} //namespace

#endif	/* MEMINFO_JNI_EXCEPTION_HPP */

