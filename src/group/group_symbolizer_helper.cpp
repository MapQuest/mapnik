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

// mapnik
#include <mapnik/group/group_symbolizer_helper.hpp>
#include <mapnik/label_collision_detector.hpp>
#include <mapnik/font_engine_freetype.hpp>
#include <mapnik/text/layout.hpp>
#include <mapnik/geom_util.hpp>
#include <mapnik/parse_path.hpp>
#include <mapnik/debug.hpp>
#include <mapnik/symbolizer.hpp>
#include <mapnik/value_types.hpp>
#include <mapnik/text/placement_finder.hpp>
#include <mapnik/text/placements/base.hpp>
#include <mapnik/text/placements/dummy.hpp>

namespace mapnik {

template <typename FaceManagerT, typename DetectorT>
group_symbolizer_helper::group_symbolizer_helper(
        const group_symbolizer &sym, const feature_impl &feature,
        const proj_transform &prj_trans,
        unsigned width, unsigned height, double scale_factor,
        const CoordTransform &t, FaceManagerT &font_manager,
        DetectorT &detector, const box2d<double> &query_extent)
    : text_symbolizer_helper(sym, feature, prj_trans, width, height,
                             scale_factor, t, font_manager, detector, query_extent)
{}

/*****************************************************************************/

template group_symbolizer_helper::group_symbolizer_helper(const group_symbolizer &sym,
    const feature_impl &feature,
    const proj_transform &prj_trans,
    unsigned width,
    unsigned height,
    double scale_factor,
    const CoordTransform &t,
    face_manager<freetype_engine> &font_manager,
    label_collision_detector4 &detector,
    const box2d<double> &query_extent);

} //namespace
