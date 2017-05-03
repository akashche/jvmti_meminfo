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
 * File:   config.hpp
 * Author: alex
 *
 * Created on April 23, 2017, 9:03 PM
 */

#ifndef MEMLOG_CONFIG_HPP
#define	MEMLOG_CONFIG_HPP

#include <string>

#include "staticlib/config.hpp"
#include "staticlib/json.hpp"

#include "memlog_exception.hpp"

namespace memlog {

class config {
public:
    std::string output_path_json;
    bool stdout_messages;
    std::string cron_expr;
    uint32_t timeout_divider;

    config(const sl::json::value& json) {
        for (auto& fi : json.as_object_or_throw("config.json")) {
            auto& name = fi.name();
            if ("output_path_json" == name) {
                output_path_json = fi.as_string_nonempty_or_throw(name);
            } else if ("stdout_messages" == name) {
                stdout_messages = fi.as_bool_or_throw(name);
            } else if ("cron_expr" == name) {
                cron_expr = fi.as_string_nonempty_or_throw(name);
            } else if ("timeout_divider" == name) {
                timeout_divider = fi.as_uint32_or_throw(name);                
            } else {
                throw memlog_exception(TRACEMSG("Invalid config field: [" + fi.name() + "]"));
            }
        }
    }
    
    sl::json::value to_json() {
        return {
            { "output_path_json", output_path_json },
            { "stdout_messages", stdout_messages },
            { "cron_expr", cron_expr },
            { "timeout_divider", timeout_divider }
        };
    }
    
};

} // namespace

#endif	/* MEMLOG_CONFIG_HPP */

