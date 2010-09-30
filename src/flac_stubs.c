/* This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Original code from libaudio-flac-decoder-perl.
 *
 * Chunks of this code have been borrowed and influenced 
 * by flac/decode.c and the flac XMMS plugin.
 *
 */


#include <memory.h>
#include <stdint.h>

#include <caml/callback.h>
#include <caml/fail.h>
#include <caml/alloc.h>
#include <caml/memory.h>
#include <caml/custom.h>
#include <caml/mlvalues.h>
#include <caml/signals.h>

#include <FLAC/stream_decoder.h>

/* polymorphic variant utility macros */
#define decl_var(x) static value var_##x
#define import_var(x) var_##x = caml_hash_variant(#x)
#define get_var(x) var_##x


/* cached polymorphic variants */
decl_var(Search_for_metadata);
decl_var(Read_metadata);
decl_var(Search_for_frame_sync);
decl_var(Read_frame);
decl_var(End_of_stream);
decl_var(Ogg_error);
decl_var(Seek_error);
decl_var(Aborted);
decl_var(Memory_allocation_error);
decl_var(Uninitialized);
decl_var(Unknown);

static value val_of_state(int s) {
  switch (s)
    {
    case FLAC__STREAM_DECODER_SEARCH_FOR_METADATA:
      return get_var(Search_for_metadata);
    case FLAC__STREAM_DECODER_READ_METADATA:
      return get_var(Read_metadata);
    case FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC:
      return get_var(Search_for_frame_sync);
    case FLAC__STREAM_DECODER_READ_FRAME:
      return get_var(Read_frame);
    case FLAC__STREAM_DECODER_END_OF_STREAM:
      return get_var(End_of_stream);
    case FLAC__STREAM_DECODER_OGG_ERROR:
      return get_var(Ogg_error);
    case FLAC__STREAM_DECODER_SEEK_ERROR:
      return get_var(Seek_error);
    case FLAC__STREAM_DECODER_ABORTED:
      return get_var(Aborted);
    case FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
      return get_var(Memory_allocation_error);
    case FLAC__STREAM_DECODER_UNINITIALIZED:
      return get_var(Uninitialized);
    default:
      return get_var(Unknown);
    }
}

/* initialize the module */
CAMLprim value ocaml_flac_stubs_initialize(value unit)
{
  CAMLparam0();
  /* initialize polymorphic variants */
  import_var(Search_for_metadata);
  import_var(Read_metadata);
  import_var(Search_for_frame_sync);
  import_var(Read_frame);
  import_var(End_of_stream);
  import_var(Ogg_error);
  import_var(Seek_error);
  import_var(Aborted);
  import_var(Memory_allocation_error);
  import_var(Uninitialized);
  import_var(Unknown);
  CAMLreturn(Val_unit);
}

typedef struct ocaml_flac_decoder_callbacks {
  value read_f;
  FLAC__int32 **out_buf;
  FLAC__Frame out_frame;
  FLAC__StreamMetadata_StreamInfo *info;
} ocaml_flac_decoder_callbacks;

typedef struct ocaml_flac_decoder {
  FLAC__StreamDecoder *decoder ;
  ocaml_flac_decoder_callbacks callbacks;
} ocaml_flac_decoder;

/* Caml abstract value containing the decoder. */
#define Decoder_val(v) (*((ocaml_flac_decoder**)Data_custom_val(v)))

static void finalize_decoder(value e)
{
  ocaml_flac_decoder *dec = Decoder_val(e);
  FLAC__stream_decoder_delete(dec->decoder);
  if (dec->callbacks.info != NULL)
    free(dec->callbacks.info);
  if (dec->callbacks.out_buf != NULL)
    free(dec->callbacks.out_buf);
  caml_remove_global_root(&dec->callbacks.read_f);
  free(dec);
}

static struct custom_operations decoder_ops =
{
  "ocaml_flac_decoder",
  finalize_decoder,
  custom_compare_default,
  custom_hash_default,
  custom_serialize_default,
  custom_deserialize_default
};

static void cpy_out_buf(ocaml_flac_decoder_callbacks *callbacks, 
                        const FLAC__int32 * const buffer[],  
                        const FLAC__Frame *frame)
{
  if (callbacks->out_buf != NULL) 
    // free previous memory pointer
    free(callbacks->out_buf);
  // TODO: raise exc if this fails.
  int samples = frame->header.blocksize;
  int channels = frame->header.channels;
  int bits_per_sample = frame->header.bits_per_sample;
  int len = sizeof(FLAC__int32)*samples*channels*bits_per_sample/8;
  callbacks->out_buf = malloc(len);
  memcpy(callbacks->out_buf,buffer,len);
  memcpy(&callbacks->out_frame,frame,sizeof(FLAC__Frame));
  return ;
}

/* start all the callbacks here. */
static void metadata_callback(const FLAC__StreamDecoder *decoder, 
                          const FLAC__StreamMetadata *metadata, 
                          void *client_data)
{
 ocaml_flac_decoder_callbacks *callbacks = (ocaml_flac_decoder_callbacks *)client_data ;
 if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
 {
    if (callbacks->info != NULL)
      free(callbacks->info);
    callbacks->info = malloc(sizeof(FLAC__StreamMetadata_StreamInfo));
    //TODO: Check alloc..
    memcpy(callbacks->info,&metadata->data.stream_info,sizeof(FLAC__StreamMetadata_StreamInfo));
 }
 return ;
}

static void error_callback(const FLAC__StreamDecoder *decoder, 
                           FLAC__StreamDecoderErrorStatus status, 
                           void *client_data)
{
 return ;
}

static FLAC__StreamDecoderSeekStatus seek_callback(const FLAC__StreamDecoder *decoder, 
                                                   FLAC__uint64 absolute_byte_offset, 
                                                   void *client_data)
{
 return FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED;
}

static FLAC__StreamDecoderTellStatus tell_callback(const FLAC__StreamDecoder *decoder, 
                                                   FLAC__uint64 *absolute_byte_offset, 
                                                   void *client_data)
{
 return FLAC__STREAM_DECODER_TELL_STATUS_UNSUPPORTED;
}

static FLAC__StreamDecoderLengthStatus length_callback(const FLAC__StreamDecoder *decoder, 
                                                       FLAC__uint64 *stream_length, 
                                                       void *client_data)
{
  return FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED;
}

static FLAC__bool eof_callback(const FLAC__StreamDecoder *decoder, void *client_data)
{
 return false;
}

static FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], 
                                   size_t *bytes, void *client_data)
{
  CAMLparam0();
  CAMLlocal1(ret);
  ocaml_flac_decoder_callbacks *callbacks = (ocaml_flac_decoder_callbacks *)client_data ;
 
  ret = caml_callback(callbacks->read_f,Val_int(*bytes));
  /* TODO: 
  ret = caml_callback_exn(callbacks->read_f,Val_int(*bytes));
  if (Is_exception_result(ret)) {
    //For now
    CAMLreturn(FLAC__STREAM_DECODER_READ_STATUS_ABORT);
  } */
  char *data = String_val(Field(ret,0));
  int len = Int_val(Field(ret,1));
  memcpy(buffer,data,len);

  CAMLreturn(FLAC__STREAM_DECODER_READ_STATUS_CONTINUE);
}

static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, 
                                                     const FLAC__Frame *frame, 
                                                     const FLAC__int32 * const buffer[], 
                                                     void *client_data)
{
  ocaml_flac_decoder_callbacks *callbacks = (ocaml_flac_decoder_callbacks *)client_data ;

  cpy_out_buf(callbacks,buffer,frame);
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

CAMLprim value ocaml_flac_decoder_create(value read_func)
{
  CAMLparam1(read_func);
  CAMLlocal1(ans);
  
  // Initialize things
  //TODO: raise exc if this fails
  ocaml_flac_decoder *dec = malloc(sizeof(ocaml_flac_decoder));
  dec->decoder = FLAC__stream_decoder_new();
  dec->callbacks.read_f = read_func;
  caml_register_global_root(&dec->callbacks.read_f);
  dec->callbacks.out_buf = NULL;
  dec->callbacks.info = NULL;
  // Intialize decoder
  FLAC__stream_decoder_init_stream(
        dec->decoder,
        read_callback,
        seek_callback,
        tell_callback,
        length_callback,
        eof_callback,
        write_callback,
        metadata_callback,
        error_callback,
        (void *)&dec->callbacks
  );

  // Process metadata
  FLAC__stream_decoder_process_until_end_of_metadata(dec->decoder);

  // Fill custom value
  ans = caml_alloc_custom(&decoder_ops, sizeof(ocaml_flac_decoder*), 1, 0);
  Decoder_val(ans) = dec;
  CAMLreturn(ans);
}

CAMLprim value ocaml_flac_decoder_state(value d)
{
  CAMLparam1(d);
  ocaml_flac_decoder *dec = Decoder_val(d);
  CAMLreturn(val_of_state(FLAC__stream_decoder_get_state(dec->decoder)));
}

CAMLprim value ocaml_flac_decoder_info(value d)
{
  CAMLparam1(d);
  CAMLlocal2(v,s);
  ocaml_flac_decoder *dec = Decoder_val(d);
  FLAC__StreamMetadata_StreamInfo *info = dec->callbacks.info;
  if (info == NULL)
    caml_raise_constant(*caml_named_value("ogg_exn_out_of_sync"));

  v = caml_alloc_tuple(5);
  Store_field(v,0,Val_int(info->sample_rate));
  Store_field(v,1,Val_int(info->channels));
  Store_field(v,2,Val_int(info->bits_per_sample));
  Store_field(v,3,caml_copy_int64(info->total_samples));
  s = caml_alloc_string(16);
  memcpy(String_val(s),info->md5sum,16);
  Store_field(v,4,s);
  CAMLreturn(v);
}

CAMLprim value ocaml_flac_decoder_read(value d)
{
  CAMLparam1(d);
  CAMLlocal1(ans);

  // This only work for S16LE (for now)
  ocaml_flac_decoder *dec = Decoder_val(d);
  ocaml_flac_decoder_callbacks *callbacks = &dec->callbacks;

  // Process one frame
  caml_enter_blocking_section();
  FLAC__stream_decoder_process_single(dec->decoder);
  caml_leave_blocking_section();

  // Alloc array
  int channels = callbacks->out_frame.header.channels;
  int samples = callbacks->out_frame.header.blocksize;
  ans = caml_alloc_tuple(channels);

  int c,i;
  for (c = 0; c < channels; c++)
    Store_field(ans, c, caml_alloc(samples * Double_wosize, Double_array_tag));

  for (c = 0; c < channels; c++)
    for (i = 0; i < samples; i++)
      Store_double_field(Field(ans, c), i, callbacks->out_buf[c][i]);

  CAMLreturn(ans);
}

CAMLprim value ocaml_flac_decoder_read_pcm(value d)
{
  CAMLparam1(d);
  CAMLlocal1(ans);

  // This only work for S16LE (for now)
  ocaml_flac_decoder *dec = Decoder_val(d);
  ocaml_flac_decoder_callbacks *callbacks = &dec->callbacks;

  // Process one frame
  caml_enter_blocking_section();
  FLAC__stream_decoder_process_single(dec->decoder);
  caml_leave_blocking_section();

  // Alloc string
  int channels = callbacks->out_frame.header.channels;
  int samples = callbacks->out_frame.header.blocksize;
  // S16_LE
  ans = caml_alloc_string(channels*samples*2);
  int16_t *pcm = (int16_t *)String_val(ans);

  int c,i;
  for (i = 0; i < samples; i++)
    for (c = 0; c < channels; c++)
      pcm[i*channels+c] = callbacks->out_buf[c][i];

  CAMLreturn(ans);
}

