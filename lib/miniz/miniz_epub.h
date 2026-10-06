#ifndef MINIZ_EPUB_H
#define MINIZ_EPUB_H

// Rename all public miniz symbols so they don't collide with
// M5GFX's bundled copy (which uses the default names).
#define mz_free                  epub_mz_free
#define mz_adler32               epub_mz_adler32
#define mz_crc32                 epub_mz_crc32
#define mz_version               epub_mz_version
#define mz_deflateInit           epub_mz_deflateInit
#define mz_deflateInit2          epub_mz_deflateInit2
#define mz_deflateReset          epub_mz_deflateReset
#define mz_deflate               epub_mz_deflate
#define mz_deflateEnd            epub_mz_deflateEnd
#define mz_deflateBound          epub_mz_deflateBound
#define mz_compress              epub_mz_compress
#define mz_compress2             epub_mz_compress2
#define mz_compressBound         epub_mz_compressBound
#define mz_inflateInit           epub_mz_inflateInit
#define mz_inflateInit2          epub_mz_inflateInit2
#define mz_inflateReset          epub_mz_inflateReset
#define mz_inflate               epub_mz_inflate
#define mz_inflateEnd            epub_mz_inflateEnd
#define mz_uncompress            epub_mz_uncompress
#define mz_uncompress2           epub_mz_uncompress2
#define mz_error                 epub_mz_error

#define tdefl_compress_mem_to_heap              epub_tdefl_compress_mem_to_heap
#define tdefl_compress_mem_to_mem               epub_tdefl_compress_mem_to_mem
#define tdefl_write_image_to_png_file_in_memory_ex  epub_tdefl_write_image_to_png_file_in_memory_ex
#define tdefl_write_image_to_png_file_in_memory epub_tdefl_write_image_to_png_file_in_memory
#define tdefl_compress_mem_to_output            epub_tdefl_compress_mem_to_output
#define tdefl_init                              epub_tdefl_init
#define tdefl_compress                          epub_tdefl_compress
#define tdefl_compress_buffer                   epub_tdefl_compress_buffer
#define tdefl_get_prev_return_status            epub_tdefl_get_prev_return_status
#define tdefl_get_adler32                       epub_tdefl_get_adler32
#define tdefl_create_comp_flags_from_zip_params epub_tdefl_create_comp_flags_from_zip_params
#define tdefl_compressor_alloc                  epub_tdefl_compressor_alloc
#define tdefl_compressor_free                   epub_tdefl_compressor_free

#define tinfl_decompress_mem_to_heap    epub_tinfl_decompress_mem_to_heap
#define tinfl_decompress_mem_to_mem     epub_tinfl_decompress_mem_to_mem
#define tinfl_decompress_mem_to_callback epub_tinfl_decompress_mem_to_callback
#define tinfl_decompressor_alloc        epub_tinfl_decompressor_alloc
#define tinfl_decompressor_free         epub_tinfl_decompressor_free
#define tinfl_decompress                epub_tinfl_decompress

#define mz_zip_zero_struct                              epub_mz_zip_zero_struct
#define mz_zip_reader_init                              epub_mz_zip_reader_init
#define mz_zip_reader_init_mem                          epub_mz_zip_reader_init_mem
#define mz_zip_reader_init_file                         epub_mz_zip_reader_init_file
#define mz_zip_reader_init_file_v2                      epub_mz_zip_reader_init_file_v2
#define mz_zip_reader_init_cfile                        epub_mz_zip_reader_init_cfile
#define mz_zip_reader_end                               epub_mz_zip_reader_end
#define mz_zip_reader_get_num_files                     epub_mz_zip_reader_get_num_files
#define mz_zip_reader_get_filename                      epub_mz_zip_reader_get_filename
#define mz_zip_reader_locate_file                       epub_mz_zip_reader_locate_file
#define mz_zip_reader_locate_file_v2                    epub_mz_zip_reader_locate_file_v2
#define mz_zip_reader_file_stat                         epub_mz_zip_reader_file_stat
#define mz_zip_reader_is_file_a_directory               epub_mz_zip_reader_is_file_a_directory
#define mz_zip_reader_is_file_encrypted                 epub_mz_zip_reader_is_file_encrypted
#define mz_zip_reader_is_file_supported                 epub_mz_zip_reader_is_file_supported
#define mz_zip_reader_extract_to_mem                    epub_mz_zip_reader_extract_to_mem
#define mz_zip_reader_extract_file_to_mem               epub_mz_zip_reader_extract_file_to_mem
#define mz_zip_reader_extract_to_heap                   epub_mz_zip_reader_extract_to_heap
#define mz_zip_reader_extract_file_to_heap              epub_mz_zip_reader_extract_file_to_heap
#define mz_zip_reader_extract_to_callback               epub_mz_zip_reader_extract_to_callback
#define mz_zip_reader_extract_file_to_callback          epub_mz_zip_reader_extract_file_to_callback
#define mz_zip_reader_extract_iter_new                  epub_mz_zip_reader_extract_iter_new
#define mz_zip_reader_extract_file_iter_new             epub_mz_zip_reader_extract_iter_file_new
#define mz_zip_reader_extract_iter_read                 epub_mz_zip_reader_extract_iter_read
#define mz_zip_reader_extract_iter_free                 epub_mz_zip_reader_extract_iter_free
#define mz_zip_get_mode                                 epub_mz_zip_get_mode
#define mz_zip_get_type                                 epub_mz_zip_get_type
#define mz_zip_get_archive_size                         epub_mz_zip_get_archive_size
#define mz_zip_get_archive_file_start_offset            epub_mz_zip_get_archive_file_start_offset
#define mz_zip_get_cfile                                epub_mz_zip_get_cfile
#define mz_zip_read_archive_data                        epub_mz_zip_read_archive_data
#define mz_zip_set_last_error                           epub_mz_zip_set_last_error
#define mz_zip_peek_last_error                          epub_mz_zip_peek_last_error
#define mz_zip_clear_last_error                         epub_mz_zip_clear_last_error
#define mz_zip_get_last_error                           epub_mz_zip_get_last_error
#define mz_zip_get_error_string                         epub_mz_zip_get_error_string
#define mz_zip_is_zip64                                 epub_mz_zip_is_zip64
#define mz_zip_get_central_dir_size                     epub_mz_zip_get_central_dir_size
#define mz_zip_end                                      epub_mz_zip_end
#define mz_zip_validate_file                            epub_mz_zip_validate_file
#define mz_zip_validate_archive                         epub_mz_zip_validate_archive
#define mz_zip_validate_mem_archive                     epub_mz_zip_validate_mem_archive
#define mz_zip_validate_file_archive                    epub_mz_zip_validate_file_archive

// Now pull in the real miniz
#include "miniz.h"

#endif
