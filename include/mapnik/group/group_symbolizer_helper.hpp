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
#ifndef GROUP_SYMBOLIZER_HELPER_HPP
#define GROUP_SYMBOLIZER_HELPER_HPP

//mapnik
#include <mapnik/text/symbolizer_helpers.hpp>
#include <mapnik/symbolizer.hpp>
#include <mapnik/feature.hpp>
#include <mapnik/marker.hpp>
#include <mapnik/marker_cache.hpp>
#include <mapnik/text/placement_finder.hpp>
#include <mapnik/proj_transform.hpp>
#include <mapnik/ctrans.hpp>

namespace mapnik {

/** Helper object that does all the TextSymbolizer placement finding
 * work except actually rendering the object. */

class group_symbolizer_helper : public text_symbolizer_helper
{
public:
    struct box_element
    {
        box_element(box2d<double> const& box, value_unicode_string const& repeat_key = "")
           : box_(box),
             repeat_key_(repeat_key)
        {}
        box2d<double> box_;
        value_unicode_string repeat_key_;
    };

    template <typename FaceManagerT, typename DetectorT>
    group_symbolizer_helper(group_symbolizer const& sym,
                            feature_impl const& feature,
                            proj_transform const& prj_trans,
                            unsigned width,
                            unsigned height,
                            double scale_factor,
                            CoordTransform const &t,
                            FaceManagerT &font_manager,
                            DetectorT &detector,
                            box2d<double> const& query_extent);

    inline void add_box_element(box2d<double> const& box, value_unicode_string const& repeat_key = "")
    {
        finder_.add_box_element(box, repeat_key);
    }

    inline void clear_box_elements()
    {
        finder_.clear_box_elements();
    }

private:

    /** Additional boxes and repeat keys to take into account when finding placement.
     * Boxes are relative to starting point of current placement.
     * Only used for point placements!
     */
    std::list<box_element> box_elements_;
};

} //namespace
#endif // GROUP_SYMBOLIZER_HELPER_HPP
