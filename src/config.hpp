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
    std::string output_path_gnuplot;

    config(const sl::json::value& json) {
       for (auto& fi : json.as_object_or_throw("config.json")) {
           auto& name = fi.name();
           if ("output_path_json" == name) {
               output_path_json = fi.as_string_nonempty_or_throw(name);
           } else if ("output_path_gnuplot" == fi.name()) {
               output_path_gnuplot = fi.as_string_nonempty_or_throw(name);
           } else {
               throw memlog_exception(TRACEMSG("Invalid config field: [" + fi.name() + "]"));
           }
       }
    }
    
    sl::json::value to_json() {
        return {
            { "output_path_json", output_path_json },
            { "output_path_gnuplot", output_path_gnuplot }
        };
    }
    
};

} // namespace

#endif	/* MEMLOG_CONFIG_HPP */

