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
 * File:   collect_mem.hpp
 * Author: alex
 *
 * Created on May 2, 2017, 10:44 PM
 */

#ifndef MEMLOG_COLLECT_MEM_HPP
#define	MEMLOG_COLLECT_MEM_HPP

#include "staticlib/json.hpp"

namespace memlog {

sl::json::value collect_mem_from_os();

} // namespace

#endif	/* COLLECT_MEM_HPP */

