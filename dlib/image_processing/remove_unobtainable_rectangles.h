// Copyright (C) 2013  Davis E. King (davis@dlib.net)
// License: Boost Software License   See LICENSE.txt for the full license.
#ifndef DLIB_REMOVE_UnOBTAINABLE_RECTANGLES_H__
#define DLIB_REMOVE_UnOBTAINABLE_RECTANGLES_H__

#include "remove_unobtainable_rectangles_abstract.h"
#include "scan_image_pyramid.h"
#include "scan_image_boxes.h"
#include "../svm/structural_object_detection_trainer.h"
#include "../geometry.h"


namespace dlib
{

// ----------------------------------------------------------------------------------------

    namespace impl
    {
        inline bool matches_rect (
            const std::vector<rectangle>& rects,
            const rectangle& rect,
            const double eps
        )
        {
            for (unsigned long i = 0; i < rects.size(); ++i)
            {
                const double score = (rect.intersect(rects[i])).area()/(double)(rect+rects[i]).area();
                if (score > eps)
                    return true;
            }

            return false;
        }

        inline rectangle get_best_matching_rect (
            const std::vector<rectangle>& rects,
            const rectangle& rect
        ) 
        {
            double best_score = -1;
            rectangle best_rect;
            for (unsigned long i = 0; i < rects.size(); ++i)
            {
                const double score = (rect.intersect(rects[i])).area()/(double)(rect+rects[i]).area();
                if (score > best_score)
                {
                    best_score = score;
                    best_rect = rects[i];
                }
            }
            return best_rect;
        }

    }

// ----------------------------------------------------------------------------------------

    template <
        typename image_array_type,
        typename Pyramid_type,
        typename Feature_extractor_type
        >
    std::vector<std::vector<rectangle> > remove_unobtainable_rectangles (
        const structural_object_detection_trainer<scan_image_pyramid<Pyramid_type, Feature_extractor_type> >& trainer,
        const image_array_type& images,
        std::vector<std::vector<rectangle> >& object_locations
    )
    {
        using namespace dlib::impl;
        // make sure requires clause is not broken
        DLIB_ASSERT(images.size() == object_locations.size(),
            "\t std::vector<std::vector<rectangle>> remove_unobtainable_rectangles()"
            << "\n\t Invalid inputs were given to this function."
            );


        std::vector<std::vector<rectangle> > rejects(images.size());

        // If the trainer is setup to automatically fit the overlap tester to the data then
        // we should use the loosest possible overlap tester here.  Otherwise we should use
        // the tester the trainer will use.
        test_box_overlap boxes_overlap(0.9999999,1); 
        if (!trainer.auto_set_overlap_tester())
            boxes_overlap = trainer.get_overlap_tester();

        for (unsigned long k = 0; k < images.size(); ++k)
        {
            std::vector<rectangle> objs = object_locations[k];

            // First remove things that don't have any matches with the candidate object
            // locations.
            std::vector<rectangle> good_rects;
            for (unsigned long j = 0; j < objs.size(); ++j)
            {
                const rectangle rect = trainer.get_scanner().get_best_matching_rect(objs[j]);
                const double score = (objs[j].intersect(rect)).area()/(double)(objs[j] + rect).area();
                if (score > trainer.get_match_eps())
                    good_rects.push_back(objs[j]);
                else
                    rejects[k].push_back(objs[j]);
            }
            object_locations[k] = good_rects;


            // Remap these rectangles to the ones that can come out of the scanner.  That
            // way when we compare them to each other in the following loop we will know if
            // any distinct truth rectangles get mapped to overlapping boxes.
            objs.resize(good_rects.size());
            for (unsigned long i = 0; i < good_rects.size(); ++i)
                objs[i] = trainer.get_scanner().get_best_matching_rect(good_rects[i]);

            good_rects.clear();
            // now check for truth rects that are too close together.
            for (unsigned long i = 0; i < objs.size(); ++i)
            {
                // check if objs[i] hits another box
                bool hit_box = false;
                for (unsigned long j = i+1; j < objs.size(); ++j)
                {
                    if (boxes_overlap(objs[i], objs[j]))
                    {
                        hit_box = true;
                        break;
                    }
                }
                if (hit_box)
                    rejects[k].push_back(object_locations[k][i]);
                else
                    good_rects.push_back(object_locations[k][i]);
            }
            object_locations[k] = good_rects;
        }

        return rejects;
    }

// ----------------------------------------------------------------------------------------

    template <
        typename image_array_type,
        typename feature_extractor, 
        typename box_generator
        >
    std::vector<std::vector<rectangle> > remove_unobtainable_rectangles (
        const structural_object_detection_trainer<scan_image_boxes<feature_extractor, box_generator> >& trainer,
        const image_array_type& images,
        std::vector<std::vector<rectangle> >& object_locations
    )
    {
        using namespace dlib::impl;
        // make sure requires clause is not broken
        DLIB_ASSERT(images.size() == object_locations.size(),
            "\t std::vector<std::vector<rectangle>> remove_unobtainable_rectangles()"
            << "\n\t Invalid inputs were given to this function."
            );

        box_generator bg = trainer.get_scanner().get_box_generator();
        std::vector<rectangle> rects;

        std::vector<std::vector<rectangle> > rejects(images.size());

        // If the trainer is setup to automatically fit the overlap tester to the data then
        // we should use the loosest possible overlap tester here.  Otherwise we should use
        // the tester the trainer will use.
        test_box_overlap boxes_overlap(0.9999999,1); 
        if (!trainer.auto_set_overlap_tester())
            boxes_overlap = trainer.get_overlap_tester();

        for (unsigned long k = 0; k < images.size(); ++k)
        {
            bg(images[k], rects);

            std::vector<rectangle> objs = object_locations[k];

            // First remove things that don't have any matches with the candidate object
            // locations.
            std::vector<rectangle> good_rects;
            for (unsigned long j = 0; j < objs.size(); ++j)
            {
                if (matches_rect(rects, objs[j], trainer.get_match_eps()))
                    good_rects.push_back(objs[j]);
                else
                    rejects[k].push_back(objs[j]);
            }
            object_locations[k] = good_rects;


            // Remap these rectangles to the ones that can come out of the scanner.  That
            // way when we compare them to each other in the following loop we will know if
            // any distinct truth rectangles get mapped to overlapping boxes.
            objs.resize(good_rects.size());
            for (unsigned long i = 0; i < good_rects.size(); ++i)
                objs[i] = get_best_matching_rect(rects, good_rects[i]);

            good_rects.clear();
            // now check for truth rects that are too close together.
            for (unsigned long i = 0; i < objs.size(); ++i)
            {
                // check if objs[i] hits another box
                bool hit_box = false;
                for (unsigned long j = i+1; j < objs.size(); ++j)
                {
                    if (boxes_overlap(objs[i], objs[j]))
                    {
                        hit_box = true;
                        break;
                    }
                }
                if (hit_box)
                    rejects[k].push_back(object_locations[k][i]);
                else
                    good_rects.push_back(object_locations[k][i]);
            }
            object_locations[k] = good_rects;
        }

        return rejects;
    }

// ----------------------------------------------------------------------------------------

}

#endif // DLIB_REMOVE_UnOBTAINABLE_RECTANGLES_H__

