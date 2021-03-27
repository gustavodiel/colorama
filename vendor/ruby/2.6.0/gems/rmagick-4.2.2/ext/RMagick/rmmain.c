/**************************************************************************//**
 * Contains all module, class, method declarations. Defines all constants.
 * Contains Magick module methods.
 *
 * Copyright &copy; 2002 - 2009 by Timothy P. Hunter
 *
 * Changes since Nov. 2009 copyright &copy; by Benjamin Thomas and Omer Bar-or
 *
 * @file     rmmain.c
 * @version  $Id: rmmain.c,v 1.303 2009/12/20 02:33:33 baror Exp $
 * @author   Tim Hunter
 ******************************************************************************/

#define MAIN                        // Define external variables
#include "rmagick.h"

#if defined(HAVE_SETMAGICKALIGNEDMEMORYMETHODS) && defined(HAVE_RB_GC_ADJUST_MEMORY_USAGE)
    #if defined(HAVE_POSIX_MEMALIGN) || defined(HAVE__ALIGNED_MSIZE)
        #define USE_RM_ALIGNED_MALLOC 1

        #if defined(HAVE_MALLOC_H)
            #include <malloc.h>
        #elif defined(HAVE_MALLOC_MALLOC_H)
            #include <malloc/malloc.h>
        #endif
    #endif
#endif

/*----------------------------------------------------------------------------\
| External declarations
\----------------------------------------------------------------------------*/
void Init_RMagick(void);

static void test_Magick_version(void);
static void version_constants(void);
static void features_constant(void);


/*
 *  Enum constants - define a subclass of Enum for the specified enumeration.
 *  Define an instance of the subclass for each member in the enumeration.
 *  Initialize each instance with its name and value.
 */
//! define Ruby enum
#define DEF_ENUM(tag) {\
   VALUE _cls, _enum;\
   _cls =  Class_##tag = rm_define_enum_type(#tag);

//! define Ruby enumerator elements
#define ENUMERATOR(val)\
   _enum = rm_enum_new(_cls, ID2SYM(rb_intern(#val)), INT2NUM(val));\
   rb_define_const(Module_Magick, #val, _enum);

//! define Ruby enumerator elements when name is different from the value
#define ENUMERATORV(name, val)\
   _enum = rm_enum_new(_cls, ID2SYM(rb_intern(#name)), INT2NUM(val));\
   rb_define_const(Module_Magick, #name, _enum);

//! end of an enumerator
#define END_ENUM }

/*
 *  Handle transferring ImageMagick memory allocations/frees to Ruby.
 *  These functions have the same signature as the equivalent C functions.
 */

/**
 * Allocate memory.
 *
 * No Ruby usage (internal function)
 *
 * @param size the size of memory to allocate
 * @return pointer to a block of memory
 */
static void *rm_malloc(size_t size)
{
    return xmalloc((long)size);
}




/**
 * Reallocate memory.
 *
 * No Ruby usage (internal function)
 *
 * @param ptr pointer to the existing block of memory
 * @param size the new size of memory to allocate
 * @return pointer to a block of memory
 */
static void *rm_realloc(void *ptr, size_t size)
{
    return xrealloc(ptr, (long)size);
}




/**
 * Free memory.
 *
 * No Ruby usage (internal function)
 *
 * @param ptr pointer to the existing block of memory
 */
static void rm_free(void *ptr)
{
    xfree(ptr);
}



#if USE_RM_ALIGNED_MALLOC

static size_t
rm_aligned_malloc_size(void *ptr)
{
#if defined(HAVE_MALLOC_USABLE_SIZE)
    return malloc_usable_size(ptr);
#elif defined(HAVE_MALLOC_SIZE)
    return malloc_size(ptr);
#elif defined(HAVE__ALIGNED_MSIZE)
// Refered to https://github.com/ImageMagick/ImageMagick/blob/master/MagickCore/memory-private.h
#define MAGICKCORE_SIZEOF_VOID_P 8
#define CACHE_LINE_SIZE  (8 * MAGICKCORE_SIZEOF_VOID_P)
    size_t _aligned_msize(void *memblock, size_t alignment, size_t offset);
    return _aligned_msize(ptr, CACHE_LINE_SIZE, 0);
#endif
}


/**
 * Allocate aligned memory.
 *
 * No Ruby usage (internal function)
 *
 * @param size the size of memory to allocate
 * @return pointer to a block of memory
 */
static void *rm_aligned_malloc(size_t size, size_t alignment)
{
    void *res;
    size_t allocated_size;

#if defined(HAVE_POSIX_MEMALIGN)
    if (posix_memalign(&res, alignment, size) != 0) {
        return NULL;
    }
#elif defined(HAVE__ALIGNED_MSIZE)
    res = _aligned_malloc(size, alignment);
#endif

    allocated_size = rm_aligned_malloc_size(res);
    rb_gc_adjust_memory_usage(allocated_size);
    return res;
}




/**
 * Free aligned memory.
 *
 * No Ruby usage (internal function)
 *
 * @param ptr pointer to the existing block of memory
 */
static void rm_aligned_free(void *ptr)
{
    size_t allocated_size = rm_aligned_malloc_size(ptr);
    rb_gc_adjust_memory_usage(-allocated_size);

#if defined(HAVE_POSIX_MEMALIGN)
    free(ptr);
#elif defined(HAVE__ALIGNED_MSIZE)
    _aligned_free(ptr);
#endif
}

#endif



/**
 * Use managed memory.
 *
 * No Ruby usage (internal function)
 */
static inline void managed_memory_enable(VALUE enable)
{
    if (enable)
    {
        SetMagickMemoryMethods(rm_malloc, rm_realloc, rm_free);
#if USE_RM_ALIGNED_MALLOC
        SetMagickAlignedMemoryMethods(rm_aligned_malloc, rm_aligned_free);
#endif
    }
    rb_define_const(Module_Magick, "MANAGED_MEMORY", enable);
}

static void set_managed_memory(void)
{
    char *disable = getenv("RMAGICK_DISABLE_MANAGED_MEMORY");

    if (disable)
    {
        managed_memory_enable(Qfalse);
        return;
    }

#if defined(_WIN32)
#if defined(IMAGEMAGICK_GREATER_THAN_EQUAL_6_9_0)
    managed_memory_enable(Qtrue);
#else
    // Disable managed memory feature with ImageMagick 6.8.x or below because causes crash.
    // Refer https://ci.appveyor.com/project/mockdeep/rmagick/builds/24706171
    managed_memory_enable(Qfalse);
#endif
#else
    // Not Windows
    managed_memory_enable(Qtrue);
#endif
}




/**
 * Define the classes and constants.
 *
 * No Ruby usage (internal function)
 */
void
Init_RMagick2(void)
{
    Module_Magick = rb_define_module("Magick");

    set_managed_memory();

    MagickCoreGenesis("RMagick", MagickFalse);

    test_Magick_version();

    rm_main_thread_id = rm_current_thread_id();

    /*-----------------------------------------------------------------------*/
    /* Create IDs for frequently used methods, etc.                          */
    /*-----------------------------------------------------------------------*/

    rm_ID_trace_proc       = rb_intern("@trace_proc");
    rm_ID_call             = rb_intern("call");
    rm_ID_changed          = rb_intern("changed");
    rm_ID_cur_image        = rb_intern("cur_image");
    rm_ID_dup              = rb_intern("dup");
    rm_ID_fill             = rb_intern("fill");
    rm_ID_Geometry         = rb_intern("Geometry");
    rm_ID_height           = rb_intern("height");
    rm_ID_initialize_copy  = rb_intern("initialize_copy");
    rm_ID_notify_observers = rb_intern("notify_observers");
    rm_ID_new              = rb_intern("new");
    rm_ID_push             = rb_intern("push");
    rm_ID_values           = rb_intern("values");
    rm_ID_width            = rb_intern("width");

    /*-----------------------------------------------------------------------*/
    /* Module Magick methods                                                 */
    /*-----------------------------------------------------------------------*/

    rb_define_module_function(Module_Magick, "colors", Magick_colors, 0);
    rb_define_module_function(Module_Magick, "fonts", Magick_fonts, 0);
    rb_define_module_function(Module_Magick, "init_formats", Magick_init_formats, 0);
    rb_define_module_function(Module_Magick, "limit_resource", Magick_limit_resource, -1);
    rb_define_module_function(Module_Magick, "set_cache_threshold", Magick_set_cache_threshold, 1);
    rb_define_module_function(Module_Magick, "set_log_event_mask", Magick_set_log_event_mask, -1);
    rb_define_module_function(Module_Magick, "set_log_format", Magick_set_log_format, 1);

    /*-----------------------------------------------------------------------*/
    /* Class Magick::Image methods                                           */
    /*-----------------------------------------------------------------------*/

    Class_Image = rb_define_class_under(Module_Magick, "Image", rb_cObject);

    // Define an alias for Object#display before we override it
    rb_define_alias(Class_Image, "__display__", "display");

    rb_define_alloc_func(Class_Image, Image_alloc);
    rb_define_method(Class_Image, "initialize", Image_initialize, -1);

    rb_define_singleton_method(Class_Image, "constitute", Image_constitute, 4);
    rb_define_singleton_method(Class_Image, "_load", Image__load, 1);
    rb_define_singleton_method(Class_Image, "capture", Image_capture, -1);
    rb_define_singleton_method(Class_Image, "ping", Image_ping, 1);
    rb_define_singleton_method(Class_Image, "read", Image_read, 1);
    rb_define_singleton_method(Class_Image, "read_inline", Image_read_inline, 1);
    rb_define_singleton_method(Class_Image, "from_blob", Image_from_blob, 1);

    // Define the attributes
    rb_define_method(Class_Image, "background_color", Image_background_color, 0);
    rb_define_method(Class_Image, "background_color=", Image_background_color_eq, 1);
    rb_define_method(Class_Image, "base_columns", Image_base_columns, 0);
    rb_define_method(Class_Image, "base_filename", Image_base_filename, 0);
    rb_define_method(Class_Image, "base_rows", Image_base_rows, 0);
    rb_define_method(Class_Image, "bias", Image_bias, 0);
    rb_define_method(Class_Image, "bias=", Image_bias_eq, 1);
    rb_define_method(Class_Image, "black_point_compensation", Image_black_point_compensation, 0);
    rb_define_method(Class_Image, "black_point_compensation=", Image_black_point_compensation_eq, 1);
    rb_define_method(Class_Image, "border_color", Image_border_color, 0);
    rb_define_method(Class_Image, "border_color=", Image_border_color_eq, 1);
    rb_define_method(Class_Image, "bounding_box", Image_bounding_box, 0);
    rb_define_method(Class_Image, "chromaticity", Image_chromaticity, 0);
    rb_define_method(Class_Image, "chromaticity=", Image_chromaticity_eq, 1);
    rb_define_method(Class_Image, "color_profile", Image_color_profile, 0);
    rb_define_method(Class_Image, "color_profile=", Image_color_profile_eq, 1);
    rb_define_method(Class_Image, "colors", Image_colors, 0);
    rb_define_method(Class_Image, "colorspace", Image_colorspace, 0);
    rb_define_method(Class_Image, "colorspace=", Image_colorspace_eq, 1);
    rb_define_method(Class_Image, "columns", Image_columns, 0);
    rb_define_method(Class_Image, "compose", Image_compose, 0);
    rb_define_method(Class_Image, "compose=", Image_compose_eq, 1);
    rb_define_method(Class_Image, "compression", Image_compression, 0);
    rb_define_method(Class_Image, "compression=", Image_compression_eq, 1);
    rb_define_method(Class_Image, "delay", Image_delay, 0);
    rb_define_method(Class_Image, "delay=", Image_delay_eq, 1);
    rb_define_method(Class_Image, "density", Image_density, 0);
    rb_define_method(Class_Image, "density=", Image_density_eq, 1);
    rb_define_method(Class_Image, "depth", Image_depth, 0);
    rb_define_method(Class_Image, "directory", Image_directory, 0);
    rb_define_method(Class_Image, "dispose", Image_dispose, 0);
    rb_define_method(Class_Image, "dispose=", Image_dispose_eq, 1);
    rb_define_method(Class_Image, "endian", Image_endian, 0);
    rb_define_method(Class_Image, "endian=", Image_endian_eq, 1);
    rb_define_method(Class_Image, "extract_info", Image_extract_info, 0);
    rb_define_method(Class_Image, "extract_info=", Image_extract_info_eq, 1);
    rb_define_method(Class_Image, "filename", Image_filename, 0);
    rb_define_method(Class_Image, "filesize", Image_filesize, 0);
    rb_define_method(Class_Image, "filter", Image_filter, 0);
    rb_define_method(Class_Image, "filter=", Image_filter_eq, 1);
    rb_define_method(Class_Image, "format", Image_format, 0);
    rb_define_method(Class_Image, "format=", Image_format_eq, 1);
    rb_define_method(Class_Image, "fuzz", Image_fuzz, 0);
    rb_define_method(Class_Image, "fuzz=", Image_fuzz_eq, 1);
    rb_define_method(Class_Image, "gamma", Image_gamma, 0);
    rb_define_method(Class_Image, "gamma=", Image_gamma_eq, 1);
    rb_define_method(Class_Image, "geometry", Image_geometry, 0);
    rb_define_method(Class_Image, "geometry=", Image_geometry_eq, 1);
    rb_define_method(Class_Image, "gravity", Image_gravity, 0);
    rb_define_method(Class_Image, "gravity=", Image_gravity_eq, 1);
    rb_define_method(Class_Image, "image_type", Image_image_type, 0);
    rb_define_method(Class_Image, "image_type=", Image_image_type_eq, 1);
    rb_define_method(Class_Image, "interlace", Image_interlace, 0);
    rb_define_method(Class_Image, "interlace=", Image_interlace_eq, 1);
    rb_define_method(Class_Image, "iptc_profile", Image_iptc_profile, 0);
    rb_define_method(Class_Image, "iptc_profile=", Image_iptc_profile_eq, 1);
    rb_define_method(Class_Image, "iterations", Image_iterations, 0);        // do not document! Only used by Image#iterations=
    rb_define_method(Class_Image, "iterations=", Image_iterations_eq, 1);        // do not document! Only used by Image#iterations=
    rb_define_method(Class_Image, "matte_color", Image_matte_color, 0);
    rb_define_method(Class_Image, "matte_color=", Image_matte_color_eq, 1);
    rb_define_method(Class_Image, "mean_error_per_pixel", Image_mean_error_per_pixel, 0);
    rb_define_method(Class_Image, "mime_type", Image_mime_type, 0);
    rb_define_method(Class_Image, "monitor=", Image_monitor_eq, 1);
    rb_define_method(Class_Image, "montage", Image_montage, 0);
    rb_define_method(Class_Image, "normalized_mean_error", Image_normalized_mean_error, 0);
    rb_define_method(Class_Image, "normalized_maximum_error", Image_normalized_maximum_error, 0);
    rb_define_method(Class_Image, "number_colors", Image_number_colors, 0);
    rb_define_method(Class_Image, "offset", Image_offset, 0);
    rb_define_method(Class_Image, "offset=", Image_offset_eq, 1);
    rb_define_method(Class_Image, "orientation", Image_orientation, 0);
    rb_define_method(Class_Image, "orientation=", Image_orientation_eq, 1);
    rb_define_method(Class_Image, "page", Image_page, 0);
    rb_define_method(Class_Image, "page=", Image_page_eq, 1);
    rb_define_method(Class_Image, "pixel_interpolation_method", Image_pixel_interpolation_method, 0);
    rb_define_method(Class_Image, "pixel_interpolation_method=", Image_pixel_interpolation_method_eq, 1);
    rb_define_method(Class_Image, "quality", Image_quality, 0);
    rb_define_method(Class_Image, "quantum_depth", Image_quantum_depth, 0);
    rb_define_method(Class_Image, "rendering_intent", Image_rendering_intent, 0);
    rb_define_method(Class_Image, "rendering_intent=", Image_rendering_intent_eq, 1);
    rb_define_method(Class_Image, "rows", Image_rows, 0);
    rb_define_method(Class_Image, "scene", Image_scene, 0);
    rb_define_method(Class_Image, "start_loop", Image_start_loop, 0);
    rb_define_method(Class_Image, "start_loop=", Image_start_loop_eq, 1);
    rb_define_method(Class_Image, "class_type", Image_class_type, 0);
    rb_define_method(Class_Image, "class_type=", Image_class_type_eq, 1);
    rb_define_method(Class_Image, "ticks_per_second", Image_ticks_per_second, 0);
    rb_define_method(Class_Image, "ticks_per_second=", Image_ticks_per_second_eq, 1);
    rb_define_method(Class_Image, "total_colors", Image_total_colors, 0);
    rb_define_method(Class_Image, "total_ink_density", Image_total_ink_density, 0);
    rb_define_method(Class_Image, "transparent_color", Image_transparent_color, 0);
    rb_define_method(Class_Image, "transparent_color=", Image_transparent_color_eq, 1);
    rb_define_method(Class_Image, "units", Image_units, 0);
    rb_define_method(Class_Image, "units=", Image_units_eq, 1);
    rb_define_method(Class_Image, "virtual_pixel_method", Image_virtual_pixel_method, 0);
    rb_define_method(Class_Image, "virtual_pixel_method=", Image_virtual_pixel_method_eq, 1);
    rb_define_method(Class_Image, "x_resolution", Image_x_resolution, 0);
    rb_define_method(Class_Image, "x_resolution=", Image_x_resolution_eq, 1);
    rb_define_method(Class_Image, "y_resolution", Image_y_resolution, 0);
    rb_define_method(Class_Image, "y_resolution=", Image_y_resolution_eq, 1);

    rb_define_method(Class_Image, "adaptive_blur", Image_adaptive_blur, -1);
    rb_define_method(Class_Image, "adaptive_blur_channel", Image_adaptive_blur_channel, -1);
    rb_define_method(Class_Image, "adaptive_resize", Image_adaptive_resize, -1);
    rb_define_method(Class_Image, "adaptive_sharpen", Image_adaptive_sharpen, -1);
    rb_define_method(Class_Image, "adaptive_sharpen_channel", Image_adaptive_sharpen_channel, -1);
    rb_define_method(Class_Image, "adaptive_threshold", Image_adaptive_threshold, -1);
    rb_define_method(Class_Image, "add_compose_mask", Image_add_compose_mask, 1);
    rb_define_method(Class_Image, "add_noise", Image_add_noise, 1);
    rb_define_method(Class_Image, "add_noise_channel", Image_add_noise_channel, -1);
    rb_define_method(Class_Image, "add_profile", Image_add_profile, 1);
    rb_define_method(Class_Image, "affine_transform", Image_affine_transform, 1);
    rb_define_method(Class_Image, "remap", Image_remap, -1);
    rb_define_method(Class_Image, "alpha", Image_alpha, -1);
    rb_define_method(Class_Image, "alpha?", Image_alpha_q, 0);
    rb_define_method(Class_Image, "[]", Image_aref, 1);
    rb_define_method(Class_Image, "[]=", Image_aset, 2);
    rb_define_method(Class_Image, "auto_gamma_channel", Image_auto_gamma_channel, -1);
    rb_define_method(Class_Image, "auto_level_channel", Image_auto_level_channel, -1);
    rb_define_method(Class_Image, "auto_orient", Image_auto_orient, 0);
    rb_define_method(Class_Image, "auto_orient!", Image_auto_orient_bang, 0);
    rb_define_method(Class_Image, "properties", Image_properties, 0);
    rb_define_method(Class_Image, "bilevel_channel", Image_bilevel_channel, -1);
    rb_define_method(Class_Image, "black_threshold", Image_black_threshold, -1);
    rb_define_method(Class_Image, "blend", Image_blend, -1);
    rb_define_method(Class_Image, "blue_shift", Image_blue_shift, -1);
    rb_define_method(Class_Image, "blur_image", Image_blur_image, -1);
    rb_define_method(Class_Image, "blur_channel", Image_blur_channel, -1);
    rb_define_method(Class_Image, "border", Image_border, 3);
    rb_define_method(Class_Image, "border!", Image_border_bang, 3);
    rb_define_method(Class_Image, "change_geometry", Image_change_geometry, 1);
    rb_define_method(Class_Image, "change_geometry!", Image_change_geometry, 1);
    rb_define_method(Class_Image, "changed?", Image_changed_q, 0);
    rb_define_method(Class_Image, "channel", Image_channel, 1);
    // An alias for compare_channel
    rb_define_method(Class_Image, "channel_compare", Image_compare_channel, -1);
    rb_define_method(Class_Image, "check_destroyed", Image_check_destroyed, 0);
    rb_define_method(Class_Image, "compare_channel", Image_compare_channel, -1);
    rb_define_method(Class_Image, "channel_depth", Image_channel_depth, -1);
    rb_define_method(Class_Image, "channel_extrema", Image_channel_extrema, -1);
    rb_define_method(Class_Image, "channel_mean", Image_channel_mean, -1);
    rb_define_method(Class_Image, "channel_entropy", Image_channel_entropy, -1);
    rb_define_method(Class_Image, "charcoal", Image_charcoal, -1);
    rb_define_method(Class_Image, "chop", Image_chop, 4);
    rb_define_method(Class_Image, "clut_channel", Image_clut_channel, -1);
    rb_define_method(Class_Image, "clone", Image_clone, 0);
    rb_define_method(Class_Image, "color_flood_fill", Image_color_flood_fill, 5);
    rb_define_method(Class_Image, "color_histogram", Image_color_histogram, 0);
    rb_define_method(Class_Image, "colorize", Image_colorize, -1);
    rb_define_method(Class_Image, "colormap", Image_colormap, -1);
    rb_define_method(Class_Image, "composite", Image_composite, -1);
    rb_define_method(Class_Image, "composite!", Image_composite_bang, -1);
    rb_define_method(Class_Image, "composite_affine", Image_composite_affine, 2);
    rb_define_method(Class_Image, "composite_channel", Image_composite_channel, -1);
    rb_define_method(Class_Image, "composite_channel!", Image_composite_channel_bang, -1);
    rb_define_method(Class_Image, "composite_mathematics", Image_composite_mathematics, -1);
    rb_define_method(Class_Image, "composite_tiled", Image_composite_tiled, -1);
    rb_define_method(Class_Image, "composite_tiled!", Image_composite_tiled_bang, -1);
    rb_define_method(Class_Image, "compress_colormap!", Image_compress_colormap_bang, 0);
    rb_define_method(Class_Image, "contrast", Image_contrast, -1);
    rb_define_method(Class_Image, "contrast_stretch_channel", Image_contrast_stretch_channel, -1);
    rb_define_method(Class_Image, "convolve", Image_convolve, 2);
    rb_define_method(Class_Image, "convolve_channel", Image_convolve_channel, -1);
    rb_define_method(Class_Image, "morphology", Image_morphology, 3);
    rb_define_method(Class_Image, "morphology_channel", Image_morphology_channel, 4);
    rb_define_method(Class_Image, "copy", Image_copy, 0);
    rb_define_method(Class_Image, "crop", Image_crop, -1);
    rb_define_method(Class_Image, "crop!", Image_crop_bang, -1);
    rb_define_method(Class_Image, "cycle_colormap", Image_cycle_colormap, 1);
    rb_define_method(Class_Image, "decipher", Image_decipher, 1);
    rb_define_method(Class_Image, "define", Image_define, 2);
    rb_define_method(Class_Image, "deskew", Image_deskew, -1);
    rb_define_method(Class_Image, "delete_compose_mask", Image_delete_compose_mask, 0);
    rb_define_method(Class_Image, "delete_profile", Image_delete_profile, 1);
    rb_define_method(Class_Image, "despeckle", Image_despeckle, 0);
    rb_define_method(Class_Image, "destroy!", Image_destroy_bang, 0);
    rb_define_method(Class_Image, "destroyed?", Image_destroyed_q, 0);
    rb_define_method(Class_Image, "difference", Image_difference, 1);
    rb_define_method(Class_Image, "dispatch", Image_dispatch, -1);
    rb_define_method(Class_Image, "displace", Image_displace, -1);
    rb_define_method(Class_Image, "display", Image_display, 0);
    rb_define_method(Class_Image, "dissolve", Image_dissolve, -1);
    rb_define_method(Class_Image, "distort", Image_distort, -1);
    rb_define_method(Class_Image, "distortion_channel", Image_distortion_channel, -1);
    rb_define_method(Class_Image, "_dump", Image__dump, 1);
    rb_define_method(Class_Image, "dup", Image_dup, 0);
    rb_define_method(Class_Image, "each_profile", Image_each_profile, 0);
    rb_define_method(Class_Image, "edge", Image_edge, -1);
    rb_define_method(Class_Image, "emboss", Image_emboss, -1);
    rb_define_method(Class_Image, "encipher", Image_encipher, 1);
    rb_define_method(Class_Image, "enhance", Image_enhance, 0);
    rb_define_method(Class_Image, "equalize", Image_equalize, 0);
    rb_define_method(Class_Image, "equalize_channel", Image_equalize_channel, -1);
    rb_define_method(Class_Image, "erase!", Image_erase_bang, 0);
    rb_define_method(Class_Image, "excerpt", Image_excerpt, 4);
    rb_define_method(Class_Image, "excerpt!", Image_excerpt_bang, 4);
    rb_define_method(Class_Image, "export_pixels", Image_export_pixels, -1);
    rb_define_method(Class_Image, "export_pixels_to_str", Image_export_pixels_to_str, -1);
    rb_define_method(Class_Image, "extent", Image_extent, -1);
    rb_define_method(Class_Image, "find_similar_region", Image_find_similar_region, -1);
    rb_define_method(Class_Image, "flip", Image_flip, 0);
    rb_define_method(Class_Image, "flip!", Image_flip_bang, 0);
    rb_define_method(Class_Image, "flop", Image_flop, 0);
    rb_define_method(Class_Image, "flop!", Image_flop_bang, 0);
    rb_define_method(Class_Image, "frame", Image_frame, -1);
    rb_define_method(Class_Image, "function_channel", Image_function_channel, -1);
    rb_define_method(Class_Image, "fx", Image_fx, -1);
    rb_define_method(Class_Image, "gamma_channel", Image_gamma_channel, -1);
    rb_define_method(Class_Image, "gamma_correct", Image_gamma_correct, -1);
    rb_define_method(Class_Image, "gaussian_blur", Image_gaussian_blur, -1);
    rb_define_method(Class_Image, "gaussian_blur_channel", Image_gaussian_blur_channel, -1);
    rb_define_method(Class_Image, "get_pixels", Image_get_pixels, 4);
    rb_define_method(Class_Image, "gray?", Image_gray_q, 0);
    rb_define_method(Class_Image, "grey?", Image_gray_q, 0);
    rb_define_method(Class_Image, "histogram?", Image_histogram_q, 0);
    rb_define_method(Class_Image, "implode", Image_implode, -1);
    rb_define_method(Class_Image, "import_pixels", Image_import_pixels, -1);
    rb_define_method(Class_Image, "initialize_copy", Image_init_copy, 1);
    rb_define_method(Class_Image, "inspect", Image_inspect, 0);
    rb_define_method(Class_Image, "level2", Image_level2, -1);
    rb_define_method(Class_Image, "level_channel", Image_level_channel, -1);
    rb_define_method(Class_Image, "level_colors", Image_level_colors, -1);
    rb_define_method(Class_Image, "levelize_channel", Image_levelize_channel, -1);
    rb_define_method(Class_Image, "linear_stretch", Image_linear_stretch, -1);
    rb_define_method(Class_Image, "liquid_rescale", Image_liquid_rescale, -1);
    rb_define_method(Class_Image, "magnify", Image_magnify, 0);
    rb_define_method(Class_Image, "magnify!", Image_magnify_bang, 0);
    rb_define_method(Class_Image, "marshal_dump", Image_marshal_dump, 0);
    rb_define_method(Class_Image, "marshal_load", Image_marshal_load, 1);
    rb_define_method(Class_Image, "mask", Image_mask, -1);
    rb_define_method(Class_Image, "matte_flood_fill", Image_matte_flood_fill, -1);
    rb_define_method(Class_Image, "median_filter", Image_median_filter, -1);
    rb_define_method(Class_Image, "minify", Image_minify, 0);
    rb_define_method(Class_Image, "minify!", Image_minify_bang, 0);
    rb_define_method(Class_Image, "modulate", Image_modulate, -1);
    rb_define_method(Class_Image, "monochrome?", Image_monochrome_q, 0);
    rb_define_method(Class_Image, "motion_blur", Image_motion_blur, -1);
    rb_define_method(Class_Image, "negate", Image_negate, -1);
    rb_define_method(Class_Image, "negate_channel", Image_negate_channel, -1);
    rb_define_method(Class_Image, "normalize", Image_normalize, 0);
    rb_define_method(Class_Image, "normalize_channel", Image_normalize_channel, -1);
    rb_define_method(Class_Image, "oil_paint", Image_oil_paint, -1);
    rb_define_method(Class_Image, "opaque", Image_opaque, 2);
    rb_define_method(Class_Image, "opaque_channel", Image_opaque_channel, -1);
    rb_define_method(Class_Image, "opaque?", Image_opaque_q, 0);
    rb_define_method(Class_Image, "ordered_dither", Image_ordered_dither, -1);
    rb_define_method(Class_Image, "paint_transparent", Image_paint_transparent, -1);
    rb_define_method(Class_Image, "palette?", Image_palette_q, 0);
    rb_define_method(Class_Image, "pixel_color", Image_pixel_color, -1);
    rb_define_method(Class_Image, "polaroid", Image_polaroid, -1);
    rb_define_method(Class_Image, "posterize", Image_posterize, -1);
//  rb_define_method(Class_Image, "plasma", Image_plasma, 6);
    rb_define_method(Class_Image, "preview", Image_preview, 1);
    rb_define_method(Class_Image, "profile!", Image_profile_bang, 2);
    rb_define_method(Class_Image, "quantize", Image_quantize, -1);
    rb_define_method(Class_Image, "quantum_operator", Image_quantum_operator, -1);
    rb_define_method(Class_Image, "radial_blur", Image_radial_blur, 1);
    rb_define_method(Class_Image, "radial_blur_channel", Image_radial_blur_channel, -1);
    rb_define_method(Class_Image, "raise", Image_raise, -1);
    rb_define_method(Class_Image, "random_threshold_channel", Image_random_threshold_channel, -1);
    rb_define_method(Class_Image, "recolor", Image_recolor, 1);
    rb_define_method(Class_Image, "reduce_noise", Image_reduce_noise, 1);
    rb_define_method(Class_Image, "resample", Image_resample, -1);
    rb_define_method(Class_Image, "resample!", Image_resample_bang, -1);
    rb_define_method(Class_Image, "resize", Image_resize, -1);
    rb_define_method(Class_Image, "resize!", Image_resize_bang, -1);
    rb_define_method(Class_Image, "roll", Image_roll, 2);
    rb_define_method(Class_Image, "rotate", Image_rotate, -1);
    rb_define_method(Class_Image, "rotate!", Image_rotate_bang, -1);
    rb_define_method(Class_Image, "sample", Image_sample, -1);
    rb_define_method(Class_Image, "sample!", Image_sample_bang, -1);
    rb_define_method(Class_Image, "scale", Image_scale, -1);
    rb_define_method(Class_Image, "scale!", Image_scale_bang, -1);
    rb_define_method(Class_Image, "segment", Image_segment, -1);
    rb_define_method(Class_Image, "selective_blur_channel", Image_selective_blur_channel, -1);
    rb_define_method(Class_Image, "separate", Image_separate, -1);
    rb_define_method(Class_Image, "sepiatone", Image_sepiatone, -1);
    rb_define_method(Class_Image, "set_channel_depth", Image_set_channel_depth, 2);
    rb_define_method(Class_Image, "shade", Image_shade, -1);
    rb_define_method(Class_Image, "shadow", Image_shadow, -1);
    rb_define_method(Class_Image, "sharpen", Image_sharpen, -1);
    rb_define_method(Class_Image, "sharpen_channel", Image_sharpen_channel, -1);
    rb_define_method(Class_Image, "shave", Image_shave, 2);
    rb_define_method(Class_Image, "shave!", Image_shave_bang, 2);
    rb_define_method(Class_Image, "shear", Image_shear, 2);
    rb_define_method(Class_Image, "sigmoidal_contrast_channel", Image_sigmoidal_contrast_channel, -1);
    rb_define_method(Class_Image, "signature", Image_signature, 0);
    rb_define_method(Class_Image, "sketch", Image_sketch, -1);
    rb_define_method(Class_Image, "solarize", Image_solarize, -1);
    rb_define_method(Class_Image, "<=>", Image_spaceship, 1);
    rb_define_method(Class_Image, "sparse_color", Image_sparse_color, -1);
    rb_define_method(Class_Image, "splice", Image_splice, -1);
    rb_define_method(Class_Image, "spread", Image_spread, -1);
    rb_define_method(Class_Image, "stegano", Image_stegano, 2);
    rb_define_method(Class_Image, "stereo", Image_stereo, 1);
    rb_define_method(Class_Image, "strip!", Image_strip_bang, 0);
    rb_define_method(Class_Image, "store_pixels", Image_store_pixels, 5);
    rb_define_method(Class_Image, "swirl", Image_swirl, 1);
    rb_define_method(Class_Image, "texture_flood_fill", Image_texture_flood_fill, 5);
    rb_define_method(Class_Image, "threshold", Image_threshold, 1);
    rb_define_method(Class_Image, "thumbnail", Image_thumbnail, -1);
    rb_define_method(Class_Image, "thumbnail!", Image_thumbnail_bang, -1);
    rb_define_method(Class_Image, "tint", Image_tint, -1);
    rb_define_method(Class_Image, "to_color", Image_to_color, 1);
    rb_define_method(Class_Image, "to_blob", Image_to_blob, 0);
    rb_define_method(Class_Image, "transparent", Image_transparent, -1);
    rb_define_method(Class_Image, "transparent_chroma", Image_transparent_chroma, -1);
    rb_define_method(Class_Image, "transpose", Image_transpose, 0);
    rb_define_method(Class_Image, "transpose!", Image_transpose_bang, 0);
    rb_define_method(Class_Image, "transverse", Image_transverse, 0);
    rb_define_method(Class_Image, "transverse!", Image_transverse_bang, 0);
    rb_define_method(Class_Image, "trim", Image_trim, -1);
    rb_define_method(Class_Image, "trim!", Image_trim_bang, -1);
    rb_define_method(Class_Image, "undefine", Image_undefine, 1);
    rb_define_method(Class_Image, "unique_colors", Image_unique_colors, 0);
    rb_define_method(Class_Image, "unsharp_mask", Image_unsharp_mask, -1);
    rb_define_method(Class_Image, "unsharp_mask_channel", Image_unsharp_mask_channel, -1);
    rb_define_method(Class_Image, "vignette", Image_vignette, -1);
    rb_define_method(Class_Image, "watermark", Image_watermark, -1);
    rb_define_method(Class_Image, "wave", Image_wave, -1);
    rb_define_method(Class_Image, "wet_floor", Image_wet_floor, -1);
    rb_define_method(Class_Image, "white_threshold", Image_white_threshold, -1);
    rb_define_method(Class_Image, "write", Image_write, 1);

    /*-----------------------------------------------------------------------*/
    /* Class Magick::ImageList methods (see also RMagick.rb)                 */
    /*-----------------------------------------------------------------------*/

    Class_ImageList = rb_define_class_under(Module_Magick, "ImageList", rb_cObject);

    // Define an alias for Object#display before we override it
    rb_define_alias(Class_ImageList, "__display__", "display");
    rb_define_method(Class_ImageList, "remap", ImageList_remap, -1);
    rb_define_method(Class_ImageList, "animate", ImageList_animate, -1);
    rb_define_method(Class_ImageList, "append", ImageList_append, 1);
    rb_define_method(Class_ImageList, "average", ImageList_average, 0);
    rb_define_method(Class_ImageList, "coalesce", ImageList_coalesce, 0);
    rb_define_method(Class_ImageList, "combine", ImageList_combine, -1);
    rb_define_method(Class_ImageList, "composite_layers", ImageList_composite_layers, -1);
    rb_define_method(Class_ImageList, "deconstruct", ImageList_deconstruct, 0);
    rb_define_method(Class_ImageList, "display", ImageList_display, 0);
    rb_define_method(Class_ImageList, "flatten_images", ImageList_flatten_images, 0);
    rb_define_method(Class_ImageList, "montage", ImageList_montage, 0);
    rb_define_method(Class_ImageList, "morph", ImageList_morph, 1);
    rb_define_method(Class_ImageList, "mosaic", ImageList_mosaic, 0);
    rb_define_method(Class_ImageList, "optimize_layers", ImageList_optimize_layers, 1);
    rb_define_method(Class_ImageList, "quantize", ImageList_quantize, -1);
    rb_define_method(Class_ImageList, "to_blob", ImageList_to_blob, 0);
    rb_define_method(Class_ImageList, "write", ImageList_write, 1);

    /*-----------------------------------------------------------------------*/
    /* Class Magick::Draw methods                                            */
    /*-----------------------------------------------------------------------*/

    Class_Draw = rb_define_class_under(Module_Magick, "Draw", rb_cObject);
    rb_define_alloc_func(Class_Draw, Draw_alloc);

    // Define the attributes
    rb_define_method(Class_Draw, "affine=", Draw_affine_eq, 1);
    rb_define_method(Class_Draw, "align=", Draw_align_eq, 1);
    rb_define_method(Class_Draw, "decorate=", Draw_decorate_eq, 1);
    rb_define_method(Class_Draw, "density=", Draw_density_eq, 1);
    rb_define_method(Class_Draw, "encoding=", Draw_encoding_eq, 1);
    rb_define_method(Class_Draw, "fill=", Draw_fill_eq, 1);
    rb_define_method(Class_Draw, "fill_pattern=", Draw_fill_pattern_eq, 1);
    rb_define_method(Class_Draw, "font=", Draw_font_eq, 1);
    rb_define_method(Class_Draw, "font_family=", Draw_font_family_eq, 1);
    rb_define_method(Class_Draw, "font_stretch=", Draw_font_stretch_eq, 1);
    rb_define_method(Class_Draw, "font_style=", Draw_font_style_eq, 1);
    rb_define_method(Class_Draw, "font_weight=", Draw_font_weight_eq, 1);
    rb_define_method(Class_Draw, "gravity=", Draw_gravity_eq, 1);
    rb_define_method(Class_Draw, "interline_spacing=", Draw_interline_spacing_eq, 1);
    rb_define_method(Class_Draw, "interword_spacing=", Draw_interword_spacing_eq, 1);
    rb_define_method(Class_Draw, "kerning=", Draw_kerning_eq, 1);
    rb_define_method(Class_Draw, "pointsize=", Draw_pointsize_eq, 1);
    rb_define_method(Class_Draw, "rotation=", Draw_rotation_eq, 1);
    rb_define_method(Class_Draw, "stroke=", Draw_stroke_eq, 1);
    rb_define_method(Class_Draw, "stroke_pattern=", Draw_stroke_pattern_eq, 1);
    rb_define_method(Class_Draw, "stroke_width=", Draw_stroke_width_eq, 1);
    rb_define_method(Class_Draw, "text_antialias=", Draw_text_antialias_eq, 1);
    rb_define_method(Class_Draw, "tile=", Draw_tile_eq, 1);
    rb_define_method(Class_Draw, "undercolor=", Draw_undercolor_eq, 1);

    rb_define_method(Class_Draw, "annotate", Draw_annotate, 6);
    rb_define_method(Class_Draw, "clone", Draw_clone, 0);
    rb_define_method(Class_Draw, "composite", Draw_composite, -1);
    rb_define_method(Class_Draw, "draw", Draw_draw, 1);
    rb_define_method(Class_Draw, "dup", Draw_dup, 0);
    rb_define_method(Class_Draw, "get_type_metrics", Draw_get_type_metrics, -1);
    rb_define_method(Class_Draw, "get_multiline_type_metrics", Draw_get_multiline_type_metrics, -1);
    rb_define_method(Class_Draw, "initialize", Draw_initialize, 0);
    rb_define_method(Class_Draw, "initialize_copy", Draw_init_copy, 1);
    rb_define_method(Class_Draw, "inspect", Draw_inspect, 0);
    rb_define_method(Class_Draw, "marshal_dump", Draw_marshal_dump, 0);
    rb_define_method(Class_Draw, "marshal_load", Draw_marshal_load, 1);
    rb_define_method(Class_Draw, "primitive", Draw_primitive, 1);

    /*-----------------------------------------------------------------------*/
    /* Class Magick::DrawOptions is identical to Magick::Draw but with       */
    /* only the attribute writer methods. This is the object that is passed  */
    /* to the block associated with the Draw.new method call.                */
    /*-----------------------------------------------------------------------*/

    Class_DrawOptions = rb_define_class_under(Class_Image, "DrawOptions", rb_cObject);

    rb_define_alloc_func(Class_DrawOptions, DrawOptions_alloc);

    rb_define_method(Class_DrawOptions, "initialize", DrawOptions_initialize, 0);

    rb_define_method(Class_DrawOptions, "affine=", Draw_affine_eq, 1);
    rb_define_method(Class_DrawOptions, "align=", Draw_align_eq, 1);
    rb_define_method(Class_DrawOptions, "decorate=", Draw_decorate_eq, 1);
    rb_define_method(Class_DrawOptions, "density=", Draw_density_eq, 1);
    rb_define_method(Class_DrawOptions, "encoding=", Draw_encoding_eq, 1);
    rb_define_method(Class_DrawOptions, "fill=", Draw_fill_eq, 1);
    rb_define_method(Class_DrawOptions, "fill_pattern=", Draw_fill_pattern_eq, 1);
    rb_define_method(Class_DrawOptions, "font=", Draw_font_eq, 1);
    rb_define_method(Class_DrawOptions, "font_family=", Draw_font_family_eq, 1);
    rb_define_method(Class_DrawOptions, "font_stretch=", Draw_font_stretch_eq, 1);
    rb_define_method(Class_DrawOptions, "font_style=", Draw_font_style_eq, 1);
    rb_define_method(Class_DrawOptions, "font_weight=", Draw_font_weight_eq, 1);
    rb_define_method(Class_DrawOptions, "gravity=", Draw_gravity_eq, 1);
    rb_define_method(Class_DrawOptions, "pointsize=", Draw_pointsize_eq, 1);
    rb_define_method(Class_DrawOptions, "rotation=", Draw_rotation_eq, 1);
    rb_define_method(Class_DrawOptions, "stroke=", Draw_stroke_eq, 1);
    rb_define_method(Class_DrawOptions, "stroke_pattern=", Draw_stroke_pattern_eq, 1);
    rb_define_method(Class_DrawOptions, "stroke_width=", Draw_stroke_width_eq, 1);
    rb_define_method(Class_DrawOptions, "text_antialias=", Draw_text_antialias_eq, 1);
    rb_define_method(Class_DrawOptions, "tile=", Draw_tile_eq, 1);
    rb_define_method(Class_DrawOptions, "undercolor=", Draw_undercolor_eq, 1);

    /*-----------------------------------------------------------------------*/
    /* Class Magick::Pixel                                                   */
    /*-----------------------------------------------------------------------*/

    Class_Pixel = rb_define_class_under(Module_Magick, "Pixel", rb_cObject);

    // include Comparable
    rb_include_module(Class_Pixel, rb_mComparable);

    // Magick::Pixel has 4 constructors: "new" "from_color", "from_hsla",
    // and the deprecated "from_HSL".
    rb_define_alloc_func(Class_Pixel, Pixel_alloc);
    rb_define_singleton_method(Class_Pixel, "from_color", Pixel_from_color, 1);
    rb_define_singleton_method(Class_Pixel, "from_hsla", Pixel_from_hsla, -1);

    // Define the RGBA attributes
    rb_define_method(Class_Pixel, "red", Pixel_red, 0);
    rb_define_method(Class_Pixel, "red=", Pixel_red_eq, 1);
    rb_define_method(Class_Pixel, "green", Pixel_green, 0);
    rb_define_method(Class_Pixel, "green=", Pixel_green_eq, 1);
    rb_define_method(Class_Pixel, "blue", Pixel_blue, 0);
    rb_define_method(Class_Pixel, "blue=", Pixel_blue_eq, 1);
    rb_define_method(Class_Pixel, "alpha", Pixel_alpha, 0);
    rb_define_method(Class_Pixel, "alpha=", Pixel_alpha_eq, 1);

    // Define the CMYK attributes
    rb_define_method(Class_Pixel, "cyan", Pixel_cyan, 0);
    rb_define_method(Class_Pixel, "cyan=", Pixel_cyan_eq, 1);
    rb_define_method(Class_Pixel, "magenta", Pixel_magenta, 0);
    rb_define_method(Class_Pixel, "magenta=", Pixel_magenta_eq, 1);
    rb_define_method(Class_Pixel, "yellow", Pixel_yellow, 0);
    rb_define_method(Class_Pixel, "yellow=", Pixel_yellow_eq, 1);
    rb_define_method(Class_Pixel, "black", Pixel_black, 0);
    rb_define_method(Class_Pixel, "black=", Pixel_black_eq, 1);


    // Define the instance methods
    rb_define_method(Class_Pixel, "<=>", Pixel_spaceship, 1);
    rb_define_method(Class_Pixel, "===", Pixel_case_eq, 1);
    rb_define_method(Class_Pixel, "eql?", Pixel_eql_q, 1);
    rb_define_method(Class_Pixel, "initialize", Pixel_initialize, -1);
    rb_define_method(Class_Pixel, "initialize_copy", Pixel_init_copy, 1);
    rb_define_method(Class_Pixel, "clone", Pixel_clone, 0);
    rb_define_method(Class_Pixel, "dup", Pixel_dup, 0);
    rb_define_method(Class_Pixel, "fcmp", Pixel_fcmp, -1);
    rb_define_method(Class_Pixel, "hash", Pixel_hash, 0);
    rb_define_method(Class_Pixel, "intensity", Pixel_intensity, 0);
    rb_define_method(Class_Pixel, "marshal_dump", Pixel_marshal_dump, 0);
    rb_define_method(Class_Pixel, "marshal_load", Pixel_marshal_load, 1);
    rb_define_method(Class_Pixel, "to_color", Pixel_to_color, -1);
    rb_define_method(Class_Pixel, "to_hsla", Pixel_to_hsla, 0);
    rb_define_method(Class_Pixel, "to_s", Pixel_to_s, 0);

    /*-----------------------------------------------------------------------*/
    /* Class Magick::ImageList::Montage methods                              */
    /*-----------------------------------------------------------------------*/

    Class_Montage = rb_define_class_under(Class_ImageList, "Montage", rb_cObject);

    rb_define_alloc_func(Class_Montage, Montage_alloc);

    rb_define_method(Class_Montage, "initialize", Montage_initialize, 0);
    rb_define_method(Class_Montage, "freeze", rm_no_freeze, 0);

    // These accessors supply optional arguments for Magick::ImageList::Montage.new
    rb_define_method(Class_Montage, "background_color=", Montage_background_color_eq, 1);
    rb_define_method(Class_Montage, "border_color=", Montage_border_color_eq, 1);
    rb_define_method(Class_Montage, "border_width=", Montage_border_width_eq, 1);
    rb_define_method(Class_Montage, "compose=", Montage_compose_eq, 1);
    rb_define_method(Class_Montage, "filename=", Montage_filename_eq, 1);
    rb_define_method(Class_Montage, "fill=", Montage_fill_eq, 1);
    rb_define_method(Class_Montage, "font=", Montage_font_eq, 1);
    rb_define_method(Class_Montage, "frame=", Montage_frame_eq, 1);
    rb_define_method(Class_Montage, "geometry=", Montage_geometry_eq, 1);
    rb_define_method(Class_Montage, "gravity=", Montage_gravity_eq, 1);
    rb_define_method(Class_Montage, "matte_color=", Montage_matte_color_eq, 1);
    rb_define_method(Class_Montage, "pointsize=", Montage_pointsize_eq, 1);
    rb_define_method(Class_Montage, "shadow=", Montage_shadow_eq, 1);
    rb_define_method(Class_Montage, "stroke=", Montage_stroke_eq, 1);
    rb_define_method(Class_Montage, "texture=", Montage_texture_eq, 1);
    rb_define_method(Class_Montage, "tile=", Montage_tile_eq, 1);
    rb_define_method(Class_Montage, "title=", Montage_title_eq, 1);

    /*-----------------------------------------------------------------------*/
    /* Class Magick::Image::Info                                             */
    /*-----------------------------------------------------------------------*/

    Class_Info = rb_define_class_under(Class_Image, "Info", rb_cObject);

    rb_define_alloc_func(Class_Info, Info_alloc);

    rb_define_method(Class_Info, "initialize", Info_initialize, 0);
    rb_define_method(Class_Info, "channel", Info_channel, -1);
    rb_define_method(Class_Info, "freeze", rm_no_freeze, 0);
    rb_define_method(Class_Info, "define", Info_define, -1);
    rb_define_method(Class_Info, "[]=", Info_aset, -1);
    rb_define_method(Class_Info, "[]", Info_aref, -1);
    rb_define_method(Class_Info, "undefine", Info_undefine, 2);

    // Define the attributes
    rb_define_method(Class_Info, "antialias", Info_antialias, 0);
    rb_define_method(Class_Info, "antialias=", Info_antialias_eq, 1);
    rb_define_method(Class_Info, "attenuate", Info_attenuate, 0);
    rb_define_method(Class_Info, "attenuate=", Info_attenuate_eq, 1);
    rb_define_method(Class_Info, "authenticate", Info_authenticate, 0);
    rb_define_method(Class_Info, "authenticate=", Info_authenticate_eq, 1);
    rb_define_method(Class_Info, "background_color", Info_background_color, 0);
    rb_define_method(Class_Info, "background_color=", Info_background_color_eq, 1);
    rb_define_method(Class_Info, "border_color", Info_border_color, 0);
    rb_define_method(Class_Info, "border_color=", Info_border_color_eq, 1);
    rb_define_method(Class_Info, "caption", Info_caption, 0);
    rb_define_method(Class_Info, "caption=", Info_caption_eq, 1);
    rb_define_method(Class_Info, "colorspace", Info_colorspace, 0);
    rb_define_method(Class_Info, "colorspace=", Info_colorspace_eq, 1);
    rb_define_method(Class_Info, "comment", Info_comment, 0);
    rb_define_method(Class_Info, "comment=", Info_comment_eq, 1);
    rb_define_method(Class_Info, "compression", Info_compression, 0);
    rb_define_method(Class_Info, "compression=", Info_compression_eq, 1);
    rb_define_method(Class_Info, "delay", Info_delay, 0);
    rb_define_method(Class_Info, "delay=", Info_delay_eq, 1);
    rb_define_method(Class_Info, "density", Info_density, 0);
    rb_define_method(Class_Info, "density=", Info_density_eq, 1);
    rb_define_method(Class_Info, "depth", Info_depth, 0);
    rb_define_method(Class_Info, "depth=", Info_depth_eq, 1);
    rb_define_method(Class_Info, "dispose", Info_dispose, 0);
    rb_define_method(Class_Info, "dispose=", Info_dispose_eq, 1);
    rb_define_method(Class_Info, "dither", Info_dither, 0);
    rb_define_method(Class_Info, "dither=", Info_dither_eq, 1);
    rb_define_method(Class_Info, "endian", Info_endian, 0);
    rb_define_method(Class_Info, "endian=", Info_endian_eq, 1);
    rb_define_method(Class_Info, "extract", Info_extract, 0);
    rb_define_method(Class_Info, "extract=", Info_extract_eq, 1);
    rb_define_method(Class_Info, "filename", Info_filename, 0);
    rb_define_method(Class_Info, "filename=", Info_filename_eq, 1);
    rb_define_method(Class_Info, "fill", Info_fill, 0);
    rb_define_method(Class_Info, "fill=", Info_fill_eq, 1);
    rb_define_method(Class_Info, "font", Info_font, 0);
    rb_define_method(Class_Info, "font=", Info_font_eq, 1);
    rb_define_method(Class_Info, "format", Info_format, 0);
    rb_define_method(Class_Info, "format=", Info_format_eq, 1);
    rb_define_method(Class_Info, "fuzz", Info_fuzz, 0);
    rb_define_method(Class_Info, "fuzz=", Info_fuzz_eq, 1);
    rb_define_method(Class_Info, "gravity", Info_gravity, 0);
    rb_define_method(Class_Info, "gravity=", Info_gravity_eq, 1);
    rb_define_method(Class_Info, "image_type", Info_image_type, 0);
    rb_define_method(Class_Info, "image_type=", Info_image_type_eq, 1);
    rb_define_method(Class_Info, "interlace", Info_interlace, 0);
    rb_define_method(Class_Info, "interlace=", Info_interlace_eq, 1);
    rb_define_method(Class_Info, "label", Info_label, 0);
    rb_define_method(Class_Info, "label=", Info_label_eq, 1);
    rb_define_method(Class_Info, "matte_color", Info_matte_color, 0);
    rb_define_method(Class_Info, "matte_color=", Info_matte_color_eq, 1);
    rb_define_method(Class_Info, "monitor=", Info_monitor_eq, 1);
    rb_define_method(Class_Info, "monochrome", Info_monochrome, 0);
    rb_define_method(Class_Info, "monochrome=", Info_monochrome_eq, 1);
    rb_define_method(Class_Info, "number_scenes", Info_number_scenes, 0);
    rb_define_method(Class_Info, "number_scenes=", Info_number_scenes_eq, 1);
    rb_define_method(Class_Info, "orientation", Info_orientation, 0);
    rb_define_method(Class_Info, "orientation=", Info_orientation_eq, 1);
    rb_define_method(Class_Info, "origin", Info_origin, 0);         // new in 6.3.1
    rb_define_method(Class_Info, "origin=", Info_origin_eq, 1);         // new in 6.3.1
    rb_define_method(Class_Info, "page", Info_page, 0);
    rb_define_method(Class_Info, "page=", Info_page_eq, 1);
    rb_define_method(Class_Info, "pointsize", Info_pointsize, 0);
    rb_define_method(Class_Info, "pointsize=", Info_pointsize_eq, 1);
    rb_define_method(Class_Info, "quality", Info_quality, 0);
    rb_define_method(Class_Info, "quality=", Info_quality_eq, 1);
    rb_define_method(Class_Info, "sampling_factor", Info_sampling_factor, 0);
    rb_define_method(Class_Info, "sampling_factor=", Info_sampling_factor_eq, 1);
    rb_define_method(Class_Info, "scene", Info_scene, 0);
    rb_define_method(Class_Info, "scene=", Info_scene_eq, 1);
    rb_define_method(Class_Info, "server_name", Info_server_name, 0);
    rb_define_method(Class_Info, "server_name=", Info_server_name_eq, 1);
    rb_define_method(Class_Info, "size", Info_size, 0);
    rb_define_method(Class_Info, "size=", Info_size_eq, 1);
    rb_define_method(Class_Info, "stroke", Info_stroke, 0);
    rb_define_method(Class_Info, "stroke=", Info_stroke_eq, 1);
    rb_define_method(Class_Info, "stroke_width", Info_stroke_width, 0);
    rb_define_method(Class_Info, "stroke_width=", Info_stroke_width_eq, 1);
    rb_define_method(Class_Info, "texture=", Info_texture_eq, 1);
    rb_define_method(Class_Info, "tile_offset", Info_tile_offset, 0);
    rb_define_method(Class_Info, "tile_offset=", Info_tile_offset_eq, 1);
    rb_define_method(Class_Info, "transparent_color", Info_transparent_color, 0);
    rb_define_method(Class_Info, "transparent_color=", Info_transparent_color_eq, 1);
    rb_define_method(Class_Info, "undercolor", Info_undercolor, 0);
    rb_define_method(Class_Info, "undercolor=", Info_undercolor_eq, 1);
    rb_define_method(Class_Info, "units", Info_units, 0);
    rb_define_method(Class_Info, "units=", Info_units_eq, 1);
    rb_define_method(Class_Info, "view", Info_view, 0);
    rb_define_method(Class_Info, "view=", Info_view_eq, 1);

    /*-----------------------------------------------------------------------*/
    /* Class Magick::KernelInfo                                              */
    /*-----------------------------------------------------------------------*/

    Class_KernelInfo = rb_define_class_under(Module_Magick, "KernelInfo", rb_cObject);

    rb_define_alloc_func(Class_KernelInfo, KernelInfo_alloc);

    rb_define_method(Class_KernelInfo, "initialize", KernelInfo_initialize, 1);
    rb_define_method(Class_KernelInfo, "unity_add", KernelInfo_unity_add, 1);
    rb_define_method(Class_KernelInfo, "scale", KernelInfo_scale, 2);
    rb_define_method(Class_KernelInfo, "scale_geometry", KernelInfo_scale_geometry, 1);
    rb_define_method(Class_KernelInfo, "clone", KernelInfo_clone, 0);
    rb_define_method(Class_KernelInfo, "dup", KernelInfo_clone, 0);

    rb_define_singleton_method(Class_KernelInfo, "builtin", KernelInfo_builtin, 2);


    /*-----------------------------------------------------------------------*/
    /* Class Magick::Image::PolaroidOptions                                  */
    /*-----------------------------------------------------------------------*/

    Class_PolaroidOptions = rb_define_class_under(Class_Image, "PolaroidOptions", rb_cObject);

    rb_define_alloc_func(Class_PolaroidOptions, PolaroidOptions_alloc);

    rb_define_method(Class_PolaroidOptions, "initialize", PolaroidOptions_initialize, 0);

    // Define the attributes
    rb_define_method(Class_PolaroidOptions, "shadow_color=", PolaroidOptions_shadow_color_eq, 1);
    rb_define_method(Class_PolaroidOptions, "border_color=", PolaroidOptions_border_color_eq, 1);

    // The other attribute writer methods are implemented by Draw's functions
    rb_define_method(Class_PolaroidOptions, "align=", Draw_align_eq, 1);
    rb_define_method(Class_PolaroidOptions, "decorate=", Draw_decorate_eq, 1);
    rb_define_method(Class_PolaroidOptions, "density=", Draw_density_eq, 1);
    rb_define_method(Class_PolaroidOptions, "encoding=", Draw_encoding_eq, 1);
    rb_define_method(Class_PolaroidOptions, "fill=", Draw_fill_eq, 1);
    rb_define_method(Class_PolaroidOptions, "fill_pattern=", Draw_fill_pattern_eq, 1);
    rb_define_method(Class_PolaroidOptions, "font=", Draw_font_eq, 1);
    rb_define_method(Class_PolaroidOptions, "font_family=", Draw_font_family_eq, 1);
    rb_define_method(Class_PolaroidOptions, "font_stretch=", Draw_font_stretch_eq, 1);
    rb_define_method(Class_PolaroidOptions, "font_style=", Draw_font_style_eq, 1);
    rb_define_method(Class_PolaroidOptions, "font_weight=", Draw_font_weight_eq, 1);
    rb_define_method(Class_PolaroidOptions, "gravity=", Draw_gravity_eq, 1);
    rb_define_method(Class_PolaroidOptions, "pointsize=", Draw_pointsize_eq, 1);
    rb_define_method(Class_PolaroidOptions, "stroke=", Draw_stroke_eq, 1);
    rb_define_method(Class_PolaroidOptions, "stroke_pattern=", Draw_stroke_pattern_eq, 1);
    rb_define_method(Class_PolaroidOptions, "stroke_width=", Draw_stroke_width_eq, 1);
    rb_define_method(Class_PolaroidOptions, "text_antialias=", Draw_text_antialias_eq, 1);
    rb_define_method(Class_PolaroidOptions, "undercolor=", Draw_undercolor_eq, 1);


    /*-----------------------------------------------------------------------*/
    /* Magick::******Fill classes and methods                                */
    /*-----------------------------------------------------------------------*/

    // class Magick::GradientFill
    Class_GradientFill = rb_define_class_under(Module_Magick, "GradientFill", rb_cObject);

    rb_define_alloc_func(Class_GradientFill, GradientFill_alloc);

    rb_define_method(Class_GradientFill, "initialize", GradientFill_initialize, 6);
    rb_define_method(Class_GradientFill, "fill", GradientFill_fill, 1);

    // class Magick::TextureFill
    Class_TextureFill = rb_define_class_under(Module_Magick, "TextureFill", rb_cObject);

    rb_define_alloc_func(Class_TextureFill, TextureFill_alloc);

    rb_define_method(Class_TextureFill, "initialize", TextureFill_initialize, 1);
    rb_define_method(Class_TextureFill, "fill", TextureFill_fill, 1);

    /*-----------------------------------------------------------------------*/
    /* Class Magick::ImageMagickError < StandardError                        */
    /* Class Magick::FatalImageMagickError < StandardError                   */
    /*-----------------------------------------------------------------------*/

    Class_ImageMagickError = rb_define_class_under(Module_Magick, "ImageMagickError", rb_eStandardError);
    rb_define_method(Class_ImageMagickError, "initialize", ImageMagickError_initialize, -1);
    rb_define_attr(Class_ImageMagickError, MAGICK_LOC, True, False);

    Class_FatalImageMagickError = rb_define_class_under(Module_Magick, "FatalImageMagickError", rb_eStandardError);


    /*-----------------------------------------------------------------------*/
    /* Class Magick::DestroyedImageError < StandardError                     */
    /*-----------------------------------------------------------------------*/
    Class_DestroyedImageError = rb_define_class_under(Module_Magick, "DestroyedImageError", rb_eStandardError);


    // Miscellaneous fixed-point constants
    DEF_CONST(QuantumRange);
    DEF_CONST(MAGICKCORE_QUANTUM_DEPTH);
    DEF_CONSTV(OpaqueAlpha, QuantumRange);
    DEF_CONSTV(TransparentAlpha, 0);

    version_constants();
    features_constant();

    /*-----------------------------------------------------------------------*/
    /* Class Magick::Enum                                                    */
    /*-----------------------------------------------------------------------*/

    // includes Comparable
    Class_Enum = rb_define_class_under(Module_Magick, "Enum", rb_cObject);
    rb_include_module(Class_Enum, rb_mComparable);

    rb_define_alloc_func(Class_Enum, Enum_alloc);

    rb_define_method(Class_Enum, "initialize", Enum_initialize, 2);
    rb_define_method(Class_Enum, "to_s", Enum_to_s, 0);
    rb_define_method(Class_Enum, "to_i", Enum_to_i, 0);
    rb_define_method(Class_Enum, "<=>", Enum_spaceship, 1);
    rb_define_method(Class_Enum, "===", Enum_case_eq, 1);
    rb_define_method(Class_Enum, "|", Enum_bitwise_or, 1);

    // AlignType constants
    DEF_ENUM(AlignType)
        ENUMERATOR(UndefinedAlign)
        ENUMERATOR(LeftAlign)
        ENUMERATOR(CenterAlign)
        ENUMERATOR(RightAlign)
    END_ENUM

    // AlphaChannelOption constants
    DEF_ENUM(AlphaChannelOption)
        ENUMERATOR(UndefinedAlphaChannel)
        ENUMERATOR(ActivateAlphaChannel)
        ENUMERATOR(DeactivateAlphaChannel)
        ENUMERATOR(SetAlphaChannel)
        ENUMERATOR(RemoveAlphaChannel)
        ENUMERATOR(CopyAlphaChannel)
        ENUMERATOR(ExtractAlphaChannel)
        ENUMERATOR(OpaqueAlphaChannel)
        ENUMERATOR(ShapeAlphaChannel)
        ENUMERATOR(TransparentAlphaChannel)
        ENUMERATOR(BackgroundAlphaChannel)
#if defined(IMAGEMAGICK_GREATER_THAN_EQUAL_6_8_9)
        ENUMERATOR(AssociateAlphaChannel)
        ENUMERATOR(DisassociateAlphaChannel)
#endif
    END_ENUM

    // AnchorType constants (for Draw#text_anchor - these are not defined by ImageMagick)
    DEF_ENUM(AnchorType)
        ENUMERATOR(StartAnchor)
        ENUMERATOR(MiddleAnchor)
        ENUMERATOR(EndAnchor)
    END_ENUM

    // ChannelType constants
    DEF_ENUM(ChannelType)
        ENUMERATOR(UndefinedChannel)
        ENUMERATOR(RedChannel)
        ENUMERATOR(CyanChannel)
        ENUMERATOR(GreenChannel)
        ENUMERATOR(MagentaChannel)
        ENUMERATOR(BlueChannel)
        ENUMERATOR(YellowChannel)
        ENUMERATOR(OpacityChannel)
        ENUMERATOR(BlackChannel)
        ENUMERATOR(IndexChannel)
        ENUMERATOR(GrayChannel)
        ENUMERATOR(AllChannels)
        ENUMERATORV(AlphaChannel, OpacityChannel)
        ENUMERATORV(DefaultChannels, 0xff & ~OpacityChannel)
        ENUMERATORV(HueChannel, RedChannel)
        ENUMERATORV(LuminosityChannel, BlueChannel)
        ENUMERATORV(SaturationChannel, GreenChannel)
    END_ENUM

    // ClassType constants
    DEF_ENUM(ClassType)
        ENUMERATOR(UndefinedClass)
        ENUMERATOR(PseudoClass)
        ENUMERATOR(DirectClass)
    END_ENUM

    // ColorspaceType constants
    DEF_ENUM(ColorspaceType)
        ENUMERATOR(UndefinedColorspace)
        ENUMERATOR(RGBColorspace)
        ENUMERATOR(GRAYColorspace)
        ENUMERATOR(TransparentColorspace)
        ENUMERATOR(OHTAColorspace)
        ENUMERATOR(XYZColorspace)
        ENUMERATOR(YCbCrColorspace)
        ENUMERATOR(YCCColorspace)
        ENUMERATOR(YIQColorspace)
        ENUMERATOR(YPbPrColorspace)
        ENUMERATOR(YUVColorspace)
        ENUMERATOR(CMYKColorspace)
        ENUMERATORV(SRGBColorspace, sRGBColorspace)
        ENUMERATOR(HSLColorspace)
        ENUMERATOR(HWBColorspace)
        ENUMERATOR(HSBColorspace)
        ENUMERATOR(LabColorspace)
        ENUMERATOR(Rec601YCbCrColorspace)
        ENUMERATOR(Rec709YCbCrColorspace)
        ENUMERATOR(LogColorspace)
        ENUMERATOR(CMYColorspace)
#if defined(IMAGEMAGICK_GREATER_THAN_EQUAL_6_8_9)
        ENUMERATOR(LuvColorspace)
        ENUMERATOR(HCLColorspace)
        ENUMERATOR(LCHColorspace)
        ENUMERATOR(LMSColorspace)
        ENUMERATOR(LCHabColorspace)
        ENUMERATOR(LCHuvColorspace)
        ENUMERATORV(ScRGBColorspace, scRGBColorspace)
        ENUMERATOR(HSIColorspace)
        ENUMERATOR(HSVColorspace)
        ENUMERATOR(HCLpColorspace)
        ENUMERATOR(YDbDrColorspace)
        ENUMERATORV(XyYColorspace, xyYColorspace)
#endif
#if defined(IMAGEMAGICK_7)
#if defined(IMAGEMAGICK_GREATER_THAN_EQUAL_7_0_8)
        ENUMERATOR(LinearGRAYColorspace)
#endif
#if defined(IMAGEMAGICK_GREATER_THAN_EQUAL_7_0_10)
        ENUMERATOR(JzazbzColorspace)
#endif
#elif defined(IMAGEMAGICK_GREATER_THAN_EQUAL_6_9_10)
        ENUMERATOR(LinearGRAYColorspace)
#endif
    END_ENUM

    // ComplianceType constants are defined as enums but used as bit flags
    DEF_ENUM(ComplianceType)
        ENUMERATOR(UndefinedCompliance)
        // AllCompliance is 0xffff, not too useful for us!
        ENUMERATORV(AllCompliance, SVGCompliance|X11Compliance|XPMCompliance)
        ENUMERATOR(NoCompliance)
        ENUMERATOR(SVGCompliance)
        ENUMERATOR(X11Compliance)
        ENUMERATOR(XPMCompliance)
    END_ENUM

    // CompositeOperator constants
    DEF_ENUM(CompositeOperator)
        ENUMERATOR(AtopCompositeOp)
        ENUMERATOR(BlendCompositeOp)
        ENUMERATOR(BlurCompositeOp)
        ENUMERATOR(BumpmapCompositeOp)
        ENUMERATOR(ChangeMaskCompositeOp)
        ENUMERATOR(ClearCompositeOp)
        ENUMERATOR(ColorBurnCompositeOp)
        ENUMERATOR(ColorDodgeCompositeOp)
        ENUMERATOR(ColorizeCompositeOp)
        ENUMERATOR(CopyBlackCompositeOp)
        ENUMERATOR(CopyBlueCompositeOp)
        ENUMERATOR(CopyCompositeOp)
        ENUMERATOR(CopyCyanCompositeOp)
        ENUMERATOR(CopyGreenCompositeOp)
        ENUMERATOR(CopyMagentaCompositeOp)
        ENUMERATOR(CopyRedCompositeOp)
        ENUMERATOR(CopyYellowCompositeOp)
        ENUMERATOR(DarkenCompositeOp)
        ENUMERATOR(DarkenIntensityCompositeOp)
        ENUMERATOR(DistortCompositeOp)
        ENUMERATOR(DivideDstCompositeOp)
        ENUMERATOR(DivideSrcCompositeOp)
        ENUMERATOR(DstAtopCompositeOp)
        ENUMERATOR(DstCompositeOp)
        ENUMERATOR(DstInCompositeOp)
        ENUMERATOR(DstOutCompositeOp)
        ENUMERATOR(DstOverCompositeOp)
        ENUMERATOR(DifferenceCompositeOp)
        ENUMERATOR(DisplaceCompositeOp)
        ENUMERATOR(DissolveCompositeOp)
        ENUMERATOR(ExclusionCompositeOp)
        ENUMERATOR(HardLightCompositeOp)
        ENUMERATOR(HueCompositeOp)
        ENUMERATOR(InCompositeOp)
        ENUMERATOR(LightenCompositeOp)
        ENUMERATOR(LightenIntensityCompositeOp)
        ENUMERATOR(LinearBurnCompositeOp)
        ENUMERATOR(LinearDodgeCompositeOp)
        ENUMERATOR(LinearLightCompositeOp)
        ENUMERATOR(LuminizeCompositeOp)
        ENUMERATOR(MathematicsCompositeOp)
        ENUMERATOR(MinusDstCompositeOp)
        ENUMERATOR(MinusSrcCompositeOp)
        ENUMERATOR(ModulateCompositeOp)
        ENUMERATOR(ModulusAddCompositeOp)
        ENUMERATOR(ModulusSubtractCompositeOp)
        ENUMERATOR(MultiplyCompositeOp)
        ENUMERATOR(NoCompositeOp)
        ENUMERATOR(OutCompositeOp)
        ENUMERATOR(OverCompositeOp)
        ENUMERATOR(OverlayCompositeOp)
        ENUMERATOR(PegtopLightCompositeOp)
        ENUMERATOR(PinLightCompositeOp)
        ENUMERATOR(PlusCompositeOp)
        ENUMERATOR(ReplaceCompositeOp)    // synonym for CopyCompositeOp
        ENUMERATOR(SaturateCompositeOp)
        ENUMERATOR(ScreenCompositeOp)
        ENUMERATOR(SoftLightCompositeOp)
        ENUMERATOR(SrcAtopCompositeOp)
        ENUMERATOR(SrcCompositeOp)
        ENUMERATOR(SrcInCompositeOp)
        ENUMERATOR(SrcOutCompositeOp)
        ENUMERATOR(SrcOverCompositeOp)
        ENUMERATOR(ThresholdCompositeOp)
        ENUMERATOR(UndefinedCompositeOp)
        ENUMERATOR(VividLightCompositeOp)
        ENUMERATOR(XorCompositeOp)
#if defined(IMAGEMAGICK_GREATER_THAN_EQUAL_6_8_9)
        ENUMERATOR(HardMixCompositeOp)
#endif
#if defined(IMAGEMAGICK_7)
        ENUMERATOR(CopyAlphaCompositeOp)
#else
        ENUMERATORV(CopyAlphaCompositeOp, CopyOpacityCompositeOp)
#endif
    END_ENUM

    // CompressionType constants
    DEF_ENUM(CompressionType)
        ENUMERATOR(UndefinedCompression)
        ENUMERATOR(NoCompression)
        ENUMERATOR(B44Compression)
        ENUMERATOR(B44ACompression)
        ENUMERATOR(BZipCompression)
        ENUMERATOR(DXT1Compression)
        ENUMERATOR(DXT3Compression)
        ENUMERATOR(DXT5Compression)
        ENUMERATOR(FaxCompression)
        ENUMERATOR(Group4Compression)
        ENUMERATOR(JPEGCompression)
        ENUMERATOR(JPEG2000Compression)
        ENUMERATOR(LosslessJPEGCompression)
        ENUMERATOR(LZWCompression)
        ENUMERATOR(PizCompression)
        ENUMERATOR(Pxr24Compression)
        ENUMERATOR(RLECompression)
        ENUMERATOR(ZipCompression)
        ENUMERATOR(ZipSCompression)
        ENUMERATOR(LZMACompression)
        ENUMERATOR(JBIG1Compression)
        ENUMERATOR(JBIG2Compression)
    END_ENUM

    // DecorationType constants
    DEF_ENUM(DecorationType)
        ENUMERATOR(NoDecoration)
        ENUMERATOR(UnderlineDecoration)
        ENUMERATOR(OverlineDecoration)
        ENUMERATOR(LineThroughDecoration)
    END_ENUM

    // DisposeType constants
    DEF_ENUM(DisposeType)
        ENUMERATOR(UndefinedDispose)
        ENUMERATOR(BackgroundDispose)
        ENUMERATOR(NoneDispose)
        ENUMERATOR(PreviousDispose)
    END_ENUM

    // DistortMethod constants
    DEF_ENUM(DistortMethod)
        ENUMERATOR(UndefinedDistortion)
        ENUMERATOR(AffineDistortion)
        ENUMERATOR(AffineProjectionDistortion)
        ENUMERATOR(ArcDistortion)
        ENUMERATOR(PolarDistortion)
        ENUMERATOR(DePolarDistortion)
        ENUMERATOR(BarrelDistortion)
        ENUMERATOR(BilinearDistortion)
        ENUMERATOR(BilinearForwardDistortion)
        ENUMERATOR(BilinearReverseDistortion)
        ENUMERATOR(PerspectiveDistortion)
        ENUMERATOR(PerspectiveProjectionDistortion)
        ENUMERATOR(PolynomialDistortion)
        ENUMERATOR(ScaleRotateTranslateDistortion)
        ENUMERATOR(ShepardsDistortion)
        ENUMERATOR(BarrelInverseDistortion)
        ENUMERATOR(Cylinder2PlaneDistortion)
        ENUMERATOR(Plane2CylinderDistortion)
        ENUMERATOR(ResizeDistortion)
        ENUMERATOR(SentinelDistortion)
    END_ENUM

    DEF_ENUM(DitherMethod)
        ENUMERATOR(UndefinedDitherMethod)
        ENUMERATOR(NoDitherMethod)
        ENUMERATOR(RiemersmaDitherMethod)
        ENUMERATOR(FloydSteinbergDitherMethod)
    END_ENUM

    DEF_ENUM(EndianType)
        ENUMERATOR(UndefinedEndian)
        ENUMERATOR(LSBEndian)
        ENUMERATOR(MSBEndian)
    END_ENUM

    // FilterType constants
    DEF_ENUM(FilterType)
        ENUMERATOR(UndefinedFilter)
        ENUMERATOR(PointFilter)
        ENUMERATOR(BoxFilter)
        ENUMERATOR(TriangleFilter)
        ENUMERATOR(HermiteFilter)
        ENUMERATOR(HanningFilter)
        ENUMERATOR(HammingFilter)
        ENUMERATOR(BlackmanFilter)
        ENUMERATOR(GaussianFilter)
        ENUMERATOR(QuadraticFilter)
        ENUMERATOR(CubicFilter)
        ENUMERATOR(CatromFilter)
        ENUMERATOR(MitchellFilter)
        ENUMERATOR(LanczosFilter)
        ENUMERATOR(BesselFilter)
        ENUMERATOR(SincFilter)
        ENUMERATOR(KaiserFilter)
        ENUMERATOR(WelshFilter)
        ENUMERATOR(ParzenFilter)
        ENUMERATOR(LagrangeFilter)
        ENUMERATOR(BohmanFilter)
        ENUMERATOR(BartlettFilter)
        ENUMERATOR(JincFilter)
        ENUMERATOR(SincFastFilter)
        ENUMERATOR(LanczosSharpFilter)
        ENUMERATOR(Lanczos2Filter)
        ENUMERATOR(Lanczos2SharpFilter)
        ENUMERATOR(RobidouxFilter)
        ENUMERATOR(RobidouxSharpFilter)
        ENUMERATOR(CosineFilter)
        ENUMERATOR(SplineFilter)
#if defined(IMAGEMAGICK_GREATER_THAN_EQUAL_6_8_9)
        ENUMERATOR(LanczosRadiusFilter)
#endif
    END_ENUM

    // GravityType constants
    DEF_ENUM(GravityType)
        ENUMERATOR(UndefinedGravity)
        ENUMERATOR(ForgetGravity)
        ENUMERATOR(NorthWestGravity)
        ENUMERATOR(NorthGravity)
        ENUMERATOR(NorthEastGravity)
        ENUMERATOR(WestGravity)
        ENUMERATOR(CenterGravity)
        ENUMERATOR(EastGravity)
        ENUMERATOR(SouthWestGravity)
        ENUMERATOR(SouthGravity)
        ENUMERATOR(SouthEastGravity)
    END_ENUM

    // ImageType constants
    DEF_ENUM(ImageType)
        ENUMERATOR(UndefinedType)
        ENUMERATOR(BilevelType)
        ENUMERATOR(GrayscaleType)
        ENUMERATOR(PaletteType)
        ENUMERATOR(TrueColorType)
        ENUMERATOR(ColorSeparationType)
        ENUMERATOR(OptimizeType)
#if defined(IMAGEMAGICK_7)
        ENUMERATOR(GrayscaleAlphaType)
        ENUMERATOR(PaletteAlphaType)
        ENUMERATOR(TrueColorAlphaType)
        ENUMERATOR(ColorSeparationAlphaType)
        ENUMERATOR(PaletteBilevelAlphaType)
#else
        ENUMERATORV(GrayscaleAlphaType, GrayscaleMatteType)
        ENUMERATORV(PaletteAlphaType, PaletteMatteType)
        ENUMERATORV(TrueColorAlphaType, TrueColorMatteType)
        ENUMERATORV(ColorSeparationAlphaType, ColorSeparationMatteType)
        ENUMERATORV(PaletteBilevelAlphaType, PaletteBilevelMatteType)
#endif
    END_ENUM

    // InterlaceType constants
    DEF_ENUM(InterlaceType)
        ENUMERATOR(UndefinedInterlace)
        ENUMERATOR(NoInterlace)
        ENUMERATOR(LineInterlace)
        ENUMERATOR(PlaneInterlace)
        ENUMERATOR(PartitionInterlace)
        ENUMERATOR(GIFInterlace)
        ENUMERATOR(JPEGInterlace)
        ENUMERATOR(PNGInterlace)
    END_ENUM

    DEF_ENUM(MagickFunction)
        ENUMERATOR(UndefinedFunction)
        ENUMERATOR(PolynomialFunction)
        ENUMERATOR(SinusoidFunction)
        ENUMERATOR(ArcsinFunction)
        ENUMERATOR(ArctanFunction)
    END_ENUM

    DEF_ENUM(LayerMethod)
        ENUMERATOR(UndefinedLayer)
        ENUMERATOR(CompareAnyLayer)
        ENUMERATOR(CompareClearLayer)
        ENUMERATOR(CompareOverlayLayer)
        ENUMERATOR(OptimizeLayer)
        ENUMERATOR(OptimizePlusLayer)
        ENUMERATOR(CoalesceLayer)
        ENUMERATOR(DisposeLayer)
        ENUMERATOR(OptimizeTransLayer)
        ENUMERATOR(OptimizeImageLayer)
        ENUMERATOR(RemoveDupsLayer)
        ENUMERATOR(RemoveZeroLayer)
        ENUMERATOR(CompositeLayer)
        ENUMERATOR(MergeLayer)
        ENUMERATOR(MosaicLayer)
        ENUMERATOR(FlattenLayer)
        ENUMERATOR(TrimBoundsLayer)
    END_ENUM

    DEF_ENUM(MetricType)
        ENUMERATOR(AbsoluteErrorMetric)
        ENUMERATOR(MeanAbsoluteErrorMetric)
        ENUMERATOR(MeanSquaredErrorMetric)
        ENUMERATOR(PeakAbsoluteErrorMetric)
        ENUMERATOR(RootMeanSquaredErrorMetric)
        ENUMERATOR(NormalizedCrossCorrelationErrorMetric)
        ENUMERATOR(FuzzErrorMetric)
#if defined(IMAGEMAGICK_GREATER_THAN_EQUAL_6_8_9)
        ENUMERATOR(PerceptualHashErrorMetric)
#endif
#if defined(IMAGEMAGICK_7)
        ENUMERATOR(UndefinedErrorMetric)
        ENUMERATOR(MeanErrorPerPixelErrorMetric)
        ENUMERATOR(PeakSignalToNoiseRatioErrorMetric)
#else
        ENUMERATORV(UndefinedErrorMetric, UndefinedMetric)
        ENUMERATORV(MeanErrorPerPixelErrorMetric, MeanErrorPerPixelMetric)
        ENUMERATORV(PeakSignalToNoiseRatioErrorMetric, PeakSignalToNoiseRatioMetric)
#endif
    END_ENUM

    // NoiseType constants
    DEF_ENUM(NoiseType)
        ENUMERATOR(UniformNoise)
        ENUMERATOR(GaussianNoise)
        ENUMERATOR(MultiplicativeGaussianNoise)
        ENUMERATOR(ImpulseNoise)
        ENUMERATOR(LaplacianNoise)
        ENUMERATOR(PoissonNoise)
        ENUMERATOR(RandomNoise)
    END_ENUM

    // Orientation constants
    DEF_ENUM(OrientationType)
        ENUMERATOR(UndefinedOrientation)
        ENUMERATOR(TopLeftOrientation)
        ENUMERATOR(TopRightOrientation)
        ENUMERATOR(BottomRightOrientation)
        ENUMERATOR(BottomLeftOrientation)
        ENUMERATOR(LeftTopOrientation)
        ENUMERATOR(RightTopOrientation)
        ENUMERATOR(RightBottomOrientation)
        ENUMERATOR(LeftBottomOrientation)
    END_ENUM

    // Paint method constants
    DEF_ENUM(PaintMethod)
        ENUMERATOR(PointMethod)
        ENUMERATOR(ReplaceMethod)
        ENUMERATOR(FloodfillMethod)
        ENUMERATOR(FillToBorderMethod)
        ENUMERATOR(ResetMethod)
    END_ENUM

    // PixelInterpolateMethod constants
    DEF_ENUM(PixelInterpolateMethod)
        ENUMERATOR(UndefinedInterpolatePixel)
        ENUMERATOR(AverageInterpolatePixel)
        ENUMERATOR(BilinearInterpolatePixel)
        ENUMERATOR(IntegerInterpolatePixel)
        ENUMERATOR(MeshInterpolatePixel)
#if defined(IMAGEMAGICK_7)
        ENUMERATOR(NearestInterpolatePixel)
#else
        ENUMERATORV(NearestInterpolatePixel, NearestNeighborInterpolatePixel)
#endif
        ENUMERATOR(SplineInterpolatePixel)
        ENUMERATOR(Average9InterpolatePixel)
        ENUMERATOR(Average16InterpolatePixel)
        ENUMERATOR(BlendInterpolatePixel)
        ENUMERATOR(BackgroundInterpolatePixel)
        ENUMERATOR(CatromInterpolatePixel)
    END_ENUM

    // PreviewType
    DEF_ENUM(PreviewType)
        ENUMERATOR(UndefinedPreview)
        ENUMERATOR(RotatePreview)
        ENUMERATOR(ShearPreview)
        ENUMERATOR(RollPreview)
        ENUMERATOR(HuePreview)
        ENUMERATOR(SaturationPreview)
        ENUMERATOR(BrightnessPreview)
        ENUMERATOR(GammaPreview)
        ENUMERATOR(SpiffPreview)
        ENUMERATOR(DullPreview)
        ENUMERATOR(GrayscalePreview)
        ENUMERATOR(QuantizePreview)
        ENUMERATOR(DespecklePreview)
        ENUMERATOR(ReduceNoisePreview)
        ENUMERATOR(AddNoisePreview)
        ENUMERATOR(SharpenPreview)
        ENUMERATOR(BlurPreview)
        ENUMERATOR(ThresholdPreview)
        ENUMERATOR(EdgeDetectPreview)
        ENUMERATOR(SpreadPreview)
        ENUMERATOR(SolarizePreview)
        ENUMERATOR(ShadePreview)
        ENUMERATOR(RaisePreview)
        ENUMERATOR(SegmentPreview)
        ENUMERATOR(SwirlPreview)
        ENUMERATOR(ImplodePreview)
        ENUMERATOR(WavePreview)
        ENUMERATOR(OilPaintPreview)
        ENUMERATOR(CharcoalDrawingPreview)
        ENUMERATOR(JPEGPreview)
    END_ENUM

    DEF_ENUM(QuantumExpressionOperator)
        ENUMERATOR(UndefinedQuantumOperator)
        ENUMERATOR(AddQuantumOperator)
        ENUMERATOR(AndQuantumOperator)
        ENUMERATOR(DivideQuantumOperator)
        ENUMERATOR(LShiftQuantumOperator)
        ENUMERATOR(MaxQuantumOperator)
        ENUMERATOR(MinQuantumOperator)
        ENUMERATOR(MultiplyQuantumOperator)
        ENUMERATOR(OrQuantumOperator)
        ENUMERATOR(RShiftQuantumOperator)
        ENUMERATOR(SubtractQuantumOperator)
        ENUMERATOR(XorQuantumOperator)
        ENUMERATOR(PowQuantumOperator)
        ENUMERATOR(LogQuantumOperator)
        ENUMERATOR(ThresholdQuantumOperator)
        ENUMERATOR(ThresholdBlackQuantumOperator)
        ENUMERATOR(ThresholdWhiteQuantumOperator)
        ENUMERATOR(GaussianNoiseQuantumOperator)
        ENUMERATOR(ImpulseNoiseQuantumOperator)
        ENUMERATOR(LaplacianNoiseQuantumOperator)
        ENUMERATOR(MultiplicativeNoiseQuantumOperator)
        ENUMERATOR(PoissonNoiseQuantumOperator)
        ENUMERATOR(UniformNoiseQuantumOperator)
        ENUMERATOR(CosineQuantumOperator)
        ENUMERATOR(SetQuantumOperator)
        ENUMERATOR(SineQuantumOperator)
        ENUMERATOR(AddModulusQuantumOperator)
        ENUMERATOR(MeanQuantumOperator)
        ENUMERATOR(AbsQuantumOperator)
        ENUMERATOR(ExponentialQuantumOperator)
        ENUMERATOR(MedianQuantumOperator)
        ENUMERATOR(SumQuantumOperator)
#if defined(IMAGEMAGICK_GREATER_THAN_EQUAL_6_8_9)
        ENUMERATOR(RootMeanSquareQuantumOperator)
#endif
    END_ENUM

    // RenderingIntent
    DEF_ENUM(RenderingIntent)
        ENUMERATOR(UndefinedIntent)
        ENUMERATOR(SaturationIntent)
        ENUMERATOR(PerceptualIntent)
        ENUMERATOR(AbsoluteIntent)
        ENUMERATOR(RelativeIntent)
    END_ENUM

    // ResolutionType constants
    DEF_ENUM(ResolutionType)
        ENUMERATOR(UndefinedResolution)
        ENUMERATOR(PixelsPerInchResolution)
        ENUMERATOR(PixelsPerCentimeterResolution)
    END_ENUM

    DEF_ENUM(SparseColorMethod)
        ENUMERATOR(UndefinedColorInterpolate)
        ENUMERATOR(BarycentricColorInterpolate)
        ENUMERATOR(BilinearColorInterpolate)
        //ENUMERATOR(PolynomialColorInterpolate)
        ENUMERATOR(ShepardsColorInterpolate)
        ENUMERATOR(VoronoiColorInterpolate)
        ENUMERATOR(InverseColorInterpolate)
    END_ENUM

    // SpreadMethod
    DEF_ENUM(SpreadMethod)
        ENUMERATOR(UndefinedSpread)
        ENUMERATOR(PadSpread)
        ENUMERATOR(ReflectSpread)
        ENUMERATOR(RepeatSpread)
    END_ENUM

    // StorageType
    DEF_ENUM(StorageType)
        ENUMERATOR(UndefinedPixel)
        ENUMERATOR(CharPixel)
        ENUMERATOR(DoublePixel)
        ENUMERATOR(FloatPixel)
        ENUMERATOR(LongPixel)
        ENUMERATOR(QuantumPixel)
        ENUMERATOR(ShortPixel)
    END_ENUM

    // StretchType constants
    DEF_ENUM(StretchType)
        ENUMERATOR(NormalStretch)
        ENUMERATOR(UltraCondensedStretch)
        ENUMERATOR(ExtraCondensedStretch)
        ENUMERATOR(CondensedStretch)
        ENUMERATOR(SemiCondensedStretch)
        ENUMERATOR(SemiExpandedStretch)
        ENUMERATOR(ExpandedStretch)
        ENUMERATOR(ExtraExpandedStretch)
        ENUMERATOR(UltraExpandedStretch)
        ENUMERATOR(AnyStretch)
    END_ENUM

    // StyleType constants
    DEF_ENUM(StyleType)
        ENUMERATOR(NormalStyle)
        ENUMERATOR(ItalicStyle)
        ENUMERATOR(ObliqueStyle)
        ENUMERATOR(AnyStyle)
    END_ENUM

    // VirtualPixelMethod
    DEF_ENUM(VirtualPixelMethod)
        ENUMERATOR(UndefinedVirtualPixelMethod)
        ENUMERATOR(EdgeVirtualPixelMethod)
        ENUMERATOR(MirrorVirtualPixelMethod)
        ENUMERATOR(TileVirtualPixelMethod)
        ENUMERATOR(TransparentVirtualPixelMethod)
        ENUMERATOR(BackgroundVirtualPixelMethod)
        ENUMERATOR(DitherVirtualPixelMethod)
        ENUMERATOR(RandomVirtualPixelMethod)
        ENUMERATOR(MaskVirtualPixelMethod)
        ENUMERATOR(BlackVirtualPixelMethod)
        ENUMERATOR(GrayVirtualPixelMethod)
        ENUMERATOR(WhiteVirtualPixelMethod)
        ENUMERATOR(HorizontalTileVirtualPixelMethod)
        ENUMERATOR(VerticalTileVirtualPixelMethod)
        ENUMERATOR(HorizontalTileEdgeVirtualPixelMethod)
        ENUMERATOR(VerticalTileEdgeVirtualPixelMethod)
        ENUMERATOR(CheckerTileVirtualPixelMethod)
    END_ENUM
    // WeightType constants
    DEF_ENUM(WeightType)
        ENUMERATOR(AnyWeight)
        ENUMERATOR(NormalWeight)
        ENUMERATOR(BoldWeight)
        ENUMERATOR(BolderWeight)
        ENUMERATOR(LighterWeight)
    END_ENUM

    // For KernelInfo scaling
    DEF_ENUM(GeometryFlags)
        ENUMERATOR(NoValue)
        ENUMERATOR(XValue)
        ENUMERATOR(XiValue)
        ENUMERATOR(YValue)
        ENUMERATOR(PsiValue)
        ENUMERATOR(WidthValue)
        ENUMERATOR(RhoValue)
        ENUMERATOR(HeightValue)
        ENUMERATOR(SigmaValue)
        ENUMERATOR(ChiValue)
        ENUMERATOR(XiNegative)
        ENUMERATOR(XNegative)
        ENUMERATOR(PsiNegative)
        ENUMERATOR(YNegative)
        ENUMERATOR(ChiNegative)
        ENUMERATOR(PercentValue)
        ENUMERATOR(AspectValue)
        ENUMERATOR(NormalizeValue)
        ENUMERATOR(LessValue)
        ENUMERATOR(GreaterValue)
        ENUMERATOR(MinimumValue)
        ENUMERATOR(CorrelateNormalizeValue)
        ENUMERATOR(AreaValue)
        ENUMERATOR(DecimalValue)
#if defined(IMAGEMAGICK_GREATER_THAN_EQUAL_6_8_9)
        ENUMERATOR(SeparatorValue)
#endif
        ENUMERATOR(AllValues)
    END_ENUM

    // Morphology methods
    DEF_ENUM(MorphologyMethod)
      ENUMERATOR(UndefinedMorphology)
      ENUMERATOR(ConvolveMorphology)
      ENUMERATOR(CorrelateMorphology)
      ENUMERATOR(ErodeMorphology)
      ENUMERATOR(DilateMorphology)
      ENUMERATOR(ErodeIntensityMorphology)
      ENUMERATOR(DilateIntensityMorphology)
      ENUMERATOR(DistanceMorphology)
      ENUMERATOR(OpenMorphology)
      ENUMERATOR(CloseMorphology)
      ENUMERATOR(OpenIntensityMorphology)
      ENUMERATOR(CloseIntensityMorphology)
      ENUMERATOR(SmoothMorphology)
      ENUMERATOR(EdgeInMorphology)
      ENUMERATOR(EdgeOutMorphology)
      ENUMERATOR(EdgeMorphology)
      ENUMERATOR(TopHatMorphology)
      ENUMERATOR(BottomHatMorphology)
      ENUMERATOR(HitAndMissMorphology)
      ENUMERATOR(ThinningMorphology)
      ENUMERATOR(ThickenMorphology)
      ENUMERATOR(VoronoiMorphology)
      ENUMERATOR(IterativeDistanceMorphology)
    END_ENUM

    DEF_ENUM(KernelInfoType)
      ENUMERATOR(UndefinedKernel)
      ENUMERATOR(UnityKernel)
      ENUMERATOR(GaussianKernel)
      ENUMERATOR(DoGKernel)
      ENUMERATOR(LoGKernel)
      ENUMERATOR(BlurKernel)
      ENUMERATOR(CometKernel)
      ENUMERATOR(LaplacianKernel)
      ENUMERATOR(SobelKernel)
      ENUMERATOR(FreiChenKernel)
      ENUMERATOR(RobertsKernel)
      ENUMERATOR(PrewittKernel)
      ENUMERATOR(CompassKernel)
      ENUMERATOR(KirschKernel)
      ENUMERATOR(DiamondKernel)
      ENUMERATOR(SquareKernel)
      ENUMERATOR(RectangleKernel)
      ENUMERATOR(OctagonKernel)
      ENUMERATOR(DiskKernel)
      ENUMERATOR(PlusKernel)
      ENUMERATOR(CrossKernel)
      ENUMERATOR(RingKernel)
      ENUMERATOR(PeaksKernel)
      ENUMERATOR(EdgesKernel)
      ENUMERATOR(CornersKernel)
      ENUMERATOR(DiagonalsKernel)
      ENUMERATOR(LineEndsKernel)
      ENUMERATOR(LineJunctionsKernel)
      ENUMERATOR(RidgesKernel)
      ENUMERATOR(ConvexHullKernel)
      ENUMERATOR(ThinSEKernel)
      ENUMERATOR(SkeletonKernel)
      ENUMERATOR(ChebyshevKernel)
      ENUMERATOR(ManhattanKernel)
      ENUMERATOR(OctagonalKernel)
      ENUMERATOR(EuclideanKernel)
      ENUMERATOR(UserDefinedKernel)
#if defined(IMAGEMAGICK_GREATER_THAN_EQUAL_6_8_9)
      ENUMERATOR(BinomialKernel)
#endif
    END_ENUM

    /*-----------------------------------------------------------------------*/
    /* Struct classes                                                        */
    /*-----------------------------------------------------------------------*/

    // Pass NULL as the structure name to keep them from polluting the Struct
    // namespace. The only way to use these classes is via the Magick:: namespace.

    // Magick::AffineMatrix
    Class_AffineMatrix = rb_struct_define_under(Module_Magick, "AffineMatrix",
                                                "sx", "rx", "ry", "sy", "tx", "ty", NULL);

    // Magick::Primary
    Class_Primary = rb_struct_define_under(Module_Magick, "Primary",
                                           "x", "y", "z", NULL);
    rb_define_method(Class_Primary, "to_s", PrimaryInfo_to_s, 0);

    // Magick::Chromaticity
    Class_Chromaticity = rb_struct_define_under(Module_Magick, "Chromaticity",
                                                "red_primary", "green_primary",
                                                "blue_primary", "white_point", NULL);
    rb_define_method(Class_Chromaticity, "to_s", ChromaticityInfo_to_s, 0);

    // Magick::Color
    Class_Color = rb_struct_define_under(Module_Magick, "Color",
                                         "name", "compliance", "color", NULL);
    rb_define_method(Class_Color, "to_s", Color_to_s, 0);

    // Magick::Point
    Class_Point = rb_struct_define_under(Module_Magick, "Point",
                                         "x", "y", NULL);

    // Magick::Rectangle
    Class_Rectangle = rb_struct_define_under(Module_Magick, "Rectangle",
                                             "width", "height", "x", "y", NULL);
    rb_define_method(Class_Rectangle, "to_s", RectangleInfo_to_s, 0);

    // Magick::Segment
    Class_Segment = rb_struct_define_under(Module_Magick, "Segment",
                                           "x1", "y1", "x2", "y2", NULL);
    rb_define_method(Class_Segment, "to_s", SegmentInfo_to_s, 0);

    // Magick::Font
    Class_Font = rb_struct_define_under(Module_Magick, "Font",
                                        "name", "description",
                                        "family", "style", "stretch", "weight",
                                        "encoding", "foundry", "format", NULL);
    rb_define_method(Class_Font, "to_s", Font_to_s, 0);

    // Magick::TypeMetric
    Class_TypeMetric = rb_struct_define_under(Module_Magick, "TypeMetric",
                                              "pixels_per_em", "ascent", "descent",
                                              "width", "height", "max_advance", "bounds",
                                              "underline_position", "underline_thickness", NULL);
    rb_define_method(Class_TypeMetric, "to_s", TypeMetric_to_s, 0);


    /*-----------------------------------------------------------------------*/
    /* Error handlers                                                        */
    /*-----------------------------------------------------------------------*/

    SetFatalErrorHandler(rm_fatal_error_handler);
    SetErrorHandler(rm_error_handler);
    SetWarningHandler(rm_warning_handler);
}




/**
 * Ensure the version of ImageMagick we're running with matches the version we
 * were compiled with.
 *
 * No Ruby usage (internal function)
 *
 * Notes:
 *   - Bypass the test by defining the constant RMAGICK_BYPASS_VERSION_TEST to
 *     'true' at the top level, before requiring 'RMagick'
 */
static void
test_Magick_version(void)
{
    size_t version_number;
    const char *version_str;
    ID bypass = rb_intern("RMAGICK_BYPASS_VERSION_TEST");

    if (RTEST(rb_const_defined(rb_cObject, bypass)) && RTEST(rb_const_get(rb_cObject, bypass)))
    {
        return;
    }

    version_str = GetMagickVersion(&version_number);
    if (version_number != MagickLibVersion)
    {
        int n, x;

        // Extract the string "ImageMagick X.Y.Z"
        n = 0;
        for (x = 0; version_str[x] != '\0'; x++)
        {
            if (version_str[x] == ' ' && ++n == 2)
            {
                break;
            }
        }

        rb_raise(rb_eRuntimeError,
                 "This installation of RMagick was configured with %s %s but %.*s is in use.\n"
                 "Please re-install RMagick to correct the issue.\n",
                 MagickPackageName, MagickLibVersionText, x, version_str);
    }

}





/**
 * Create Version, Magick_version, and Version_long constants.
 *
 * No Ruby usage (internal function)
 */
static void
version_constants(void)
{
    const char *mgk_version;
    VALUE str;
    char long_version[1000];

    mgk_version = GetMagickVersion(NULL);

    str = rb_str_new2(mgk_version);
    rb_obj_freeze(str);
    rb_define_const(Module_Magick, "Magick_version", str);

    str = rb_str_new2(Q(RMAGICK_VERSION_STRING));
    rb_obj_freeze(str);
    rb_define_const(Module_Magick, "Version", str);

    snprintf(long_version, sizeof(long_version),
            "This is %s ($Date: 2009/12/20 02:33:33 $) Copyright (C) 2009 by Timothy P. Hunter\n"
            "Built with %s\n"
            "Built for %s\n"
            "Web page: http://rmagick.rubyforge.org\n"
            "Email: rmagick@rubyforge.org\n",
            Q(RMAGICK_VERSION_STRING), mgk_version, Q(RUBY_VERSION_STRING));

    str = rb_str_new2(long_version);
    rb_obj_freeze(str);
    rb_define_const(Module_Magick, "Long_version", str);

    RB_GC_GUARD(str);
}


/**
 * Create Features constant.
 *
 * No Ruby usage (internal function)
 */
static void
features_constant(void)
{
    VALUE features;

    // 6.5.7 - latest (7.0.0)
    features = rb_str_new2(GetMagickFeatures());

    rb_obj_freeze(features);
    rb_define_const(Module_Magick, "Magick_features", features);

    RB_GC_GUARD(features);
}
