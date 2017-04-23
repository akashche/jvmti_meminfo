/* 
 * File:   memlog_exception.hpp
 * Author: alex
 *
 * Created on April 23, 2017, 9:06 PM
 */

#ifndef MEMLOG_MEMLOG_EXCEPTION_HPP
#define	MEMLOG_MEMLOG_EXCEPTION_HPP

#include "staticlib/support/exception.hpp"

namespace memlog {

/**
 * Module specific exception
 */
class memlog_exception : public sl::support::exception {
public:
    /**
     * Default constructor
     */
    memlog_exception() = default;

    /**
     * Constructor with message
     * 
     * @param msg error message
     */
    memlog_exception(const std::string& msg) :
    sl::support::exception(msg) { }

};

} //namespace

#endif	/* MEMLOG_MEMLOG_EXCEPTION_HPP */

