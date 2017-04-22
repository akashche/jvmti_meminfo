/* 
 * File:   jvmti_exception.hpp
 * Author: alex
 *
 * Created on April 13, 2017, 10:38 AM
 */

#ifndef MEMINFO_JVMTI_EXCEPTION_HPP
#define	MEMINFO_JVMTI_EXCEPTION_HPP

#include "staticlib/support/exception.hpp"

namespace jvmti_helper {

/**
 * Module specific exception
 */
class jvmti_exception : public sl::support::exception {
public:
    /**
     * Default constructor
     */
    jvmti_exception() = default;

    /**
     * Constructor with message
     * 
     * @param msg error message
     */
    jvmti_exception(const std::string& msg) :
    sl::support::exception(msg) { }

};

} //namespace

#endif	/* MEMINFO_JVMTI_EXCEPTION_HPP */

