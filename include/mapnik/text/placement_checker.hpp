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
#ifndef MAPNIK_PLACEMENT_CHECKER_HPP
#define MAPNIK_PLACEMENT_CHECKER_HPP

//mapnik
#include <mapnik/box2d.hpp>
#include <mapnik/pixel_position.hpp>
#include <mapnik/text/placements/base.hpp>
#include <mapnik/text/placements_list.hpp>
#include <mapnik/text/rotation.hpp>
#include <mapnik/noncopyable.hpp>

namespace mapnik
{

class label_collision_detector4;
class text_layout;
typedef label_collision_detector4 DetectorType;

/**
 * Checks a placement against a detector, updates the detector 
 * and builds a glyph_positions_ptr structure to be rendered.
 *
 * This is separated into several functions so that the logic
 * of whether something is placed or not can be composed with
 * other objects. It allows the addition of external checks,
 * such as the placement of other objects, so that placement
 * decisions can be more sophisticated than just whether this
 * object can be placed or not.
 */
class placement_checker : public mapnik::noncopyable
{
public:
    placement_checker(text_layout const &layout,
                      text_placement_info_ptr info,
                      rotation const &orientation,
                      justify_alignment_e jalign,
                      bool has_marker,
                      double scale_factor,
                      box2d<double> const &extent,
                      marker_info_ptr marker,
                      pixel_position const &pos,
                      pixel_position const &alignment_offset,
                      pixel_position const &marker_displacement,
                      bool marker_unlocked,
                      box2d<double> const &marker_box);

    // Returns true if the text layout and marker can be placed
    // in the given detector.
    bool has_placement(DetectorType &detector) const;

    // Adds the text layout and marker to the detector.
    void add_to_detector(DetectorType &detector) const;

    // Returns the glyph_positions_ptr representing the positions
    // of the text layout and marker. Note that the computation is
    // performed in this function, so it should not be called
    // unless you plan to use the returned positions.
    glyph_positions_ptr get_positions() const;

private:
    // reference information
    text_layout const &layout_;
    text_placement_info_ptr info_;
    rotation orientation_;
    justify_alignment_e jalign_;
    bool has_marker_;
    double scale_factor_;
    box2d<double> extent_;
    marker_info_ptr marker_;
    
    // internal computed information
    pixel_position base_point_, marker_real_pos_;
    box2d<double> text_bbox_, marker_bbox_;

    // compute justification offset from line width
    double jalign_offset(double line_width) const;

    // returns true if the given box collides with something in
    // the detector.
    bool collision(DetectorType &detector, const box2d<double> &box) const;
};

} //ns mapnik

#endif // PLACEMENT_CHECKER_HPP
