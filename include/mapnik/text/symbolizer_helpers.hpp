/*****************************************************************************
 *
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2013 Artem Pavlenko
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/
#ifndef SYMBOLIZER_HELPERS_HPP
#define SYMBOLIZER_HELPERS_HPP

//mapnik
#include <mapnik/symbolizer.hpp>
#include <mapnik/feature.hpp>
#include <mapnik/marker.hpp>
#include <mapnik/marker_cache.hpp>
#include <mapnik/text/placement_finder.hpp>
#include <mapnik/proj_transform.hpp>
#include <mapnik/ctrans.hpp>

namespace mapnik {

// forward declare used classes
class label_collision_detector4;
template <typename T> class face_manager;
class freetype_engine;

/** Helper object that does all the TextSymbolizer placement finding
 * work except actually rendering the object. */

class text_symbolizer_helper
{
public:
    text_symbolizer_helper(text_symbolizer const& sym,
                           feature_impl const& feature,
                           proj_transform const& prj_trans,
                           unsigned width,
                           unsigned height,
                           double scale_factor,
                           CoordTransform const& t,
                           face_manager<freetype_engine> &font_manager,
                           label_collision_detector4 &detector,
                           box2d<double> const& query_extent);

    text_symbolizer_helper(shield_symbolizer const& sym,
                           feature_impl const& feature,
                           proj_transform const& prj_trans,
                           unsigned width,
                           unsigned height,
                           double scale_factor,
                           CoordTransform const &t,
                           face_manager<freetype_engine> &font_manager,
                           label_collision_detector4 &detector,
                           box2d<double> const& query_extent,
                           bool use_default_marker = false);

    text_symbolizer_helper(point_symbolizer const& sym,
                           feature_impl const& feature,
                           proj_transform const& prj_trans,
                           unsigned width,
                           unsigned height,
                           double scale_factor,
                           CoordTransform const &t,
                           face_manager<freetype_engine> &font_manager,
                           label_collision_detector4 &detector,
                           box2d<double> const& query_extent,
                           bool use_default_marker = false);

    /** Return all placements.*/
    placements_list const& get();
protected:
    bool next_point_placement();
    bool next_line_placement(bool clipped);
    void initialize_geometries();
    void initialize_points(label_placement_enum how_placed);

    //Input
    symbolizer_base const& sym_;
    feature_impl const& feature_;
    proj_transform const& prj_trans_;
    CoordTransform const& t_;
    box2d<double> dims_;
    box2d<double> const& query_extent_;
    //Processing
    /* Using list instead of vector, because we delete random elements and need iterators to stay valid. */
    /** Remaining geometries to be processed. */
    std::list<geometry_type*> geometries_to_process_;
    /** Geometry currently being processed. */
    std::list<geometry_type*>::iterator geo_itr_;
    /** Remaining points to be processed. */
    std::list<pixel_position> points_;
    /** Point currently being processed. */
    std::list<pixel_position>::iterator point_itr_;
    /** Use point placement. Otherwise line placement is used. */
    bool point_placement_;
    /** Place text at points on a line instead of following the line (used for ShieldSymbolizer) .*/
    bool points_on_line_;

    text_placement_info_ptr placement_;
    collidable_properties collidable_properties_;
    placement_finder finder_;

    //ShieldSymbolizer only
    void init_marker(bool use_default_marker);
};

} //namespace
#endif // SYMBOLIZER_HELPERS_HPP
