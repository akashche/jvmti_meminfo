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

