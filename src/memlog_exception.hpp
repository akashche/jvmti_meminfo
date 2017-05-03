/*
 * Copyright 2017, akashche at redhat.com
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE.md file that
 * accompanied this code).
 */

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

