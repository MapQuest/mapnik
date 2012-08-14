/*****************************************************************************
 *
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2011 Artem Pavlenko
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
#include <mapnik/agg_renderer.hpp>
#include <mapnik/agg_rasterizer.hpp>
#include <mapnik/image_util.hpp>

#include <mapnik/geom_util.hpp>
#include <mapnik/point_symbolizer.hpp>
#include <mapnik/expression_evaluator.hpp>
#include <mapnik/marker.hpp>
#include <mapnik/marker_cache.hpp>
#include <mapnik/expression_evaluator.hpp>
#include <mapnik/symbolizer_helpers.hpp>

// agg
#include "agg_trans_affine.h"

// stl
#include <string>

// boost
#include <boost/make_shared.hpp>

namespace mapnik {

template <typename T>
void agg_renderer<T>::process(point_symbolizer const& sym,
                              mapnik::feature_impl & feature,
                              proj_transform const& prj_trans)
{
    symbolizer_with_image_helper helper(sym, feature, scale_factor_);

    if (helper.get_marker())
    {
        box2d<double> label_ext = helper.get_label_ext();
        mapnik::marker const& marker = **helper.get_marker();

        for (unsigned i=0; i<feature.num_geometries(); ++i)
        {
            geometry_type const& geom = feature.get_geometry(i);
            double x;
            double y;
            double z=0;
            if (sym.get_point_placement() == CENTROID_POINT_PLACEMENT)
                label::centroid(geom, x, y);
            else
                label::interior_position(geom ,x, y);

            prj_trans.backward(x,y,z);
            t_.forward(&x,&y);
            label_ext.re_center(x,y);
            
            if (sym.get_allow_overlap() ||
                detector_->has_placement(label_ext))
            {
                render_marker(pixel_position(x, y),
                              marker,
                              helper.get_transform(),
                              sym.get_opacity(),
                              sym.comp_op());

                if (/* DEBUG */ 0) {
                    debug_draw_box(label_ext, 0, 0, 0.0);
                }

                if (!sym.get_ignore_placement())
                    detector_->insert(label_ext);
                //metawriter_with_properties writer = sym.get_metawriter();
                //if (writer.first) writer.first->add_box(label_ext, feature, t_, writer.second);
            }
        }
    }

}

template void agg_renderer<image_32>::process(point_symbolizer const&,
                                              mapnik::feature_impl &,
                                              proj_transform const&);

}
