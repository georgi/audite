#include <ruby.h>
#include <math.h>
#include <pthread.h>
#include <portaudio.h>
#include <mpg123.h>

typedef struct {
  PaStream *stream;
  int size;
  float *buffer;
  float rms;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} Portaudio;

static VALUE rb_cPortaudio;

void free_portaudio(void *ptr)
{
  Portaudio *portaudio = (Portaudio *) ptr;

  if (portaudio) {
    pthread_cond_destroy(&portaudio->cond);
    pthread_mutex_destroy(&portaudio->mutex);

    if (portaudio->buffer) {
      free(portaudio->buffer);
    }

    Pa_CloseStream(portaudio->stream);
  }
}

static int paCallback(const void *inputBuffer,
                      void *outputBuffer,
                      unsigned long framesPerBuffer,
                      const PaStreamCallbackTimeInfo* timeInfo,
                      PaStreamCallbackFlags statusFlags,
                      void *userData )
{
  Portaudio *portaudio = (Portaudio *) userData;
  float *out = (float*) outputBuffer;
  unsigned int i;

  pthread_mutex_lock(&portaudio->mutex);

  memcpy(out, portaudio->buffer, sizeof(float) * portaudio->size);

  pthread_cond_broadcast(&portaudio->cond);
  pthread_mutex_unlock(&portaudio->mutex);

  return 0;
}

VALUE rb_portaudio_new(VALUE klass, VALUE framesPerBuffer)
{
  PaStreamParameters outputParameters;
  const PaDeviceInfo *deviceInfo;
  PaError err;
  int device, numDevices;
  VALUE self;
  Portaudio *portaudio = (Portaudio *) malloc(sizeof(Portaudio));

  pthread_mutex_init(&portaudio->mutex, NULL);
  pthread_cond_init(&portaudio->cond, NULL);

  portaudio->size = FIX2INT(framesPerBuffer) * 2;
  portaudio->buffer = (float *) malloc(sizeof(float) * portaudio->size);

  err = Pa_OpenDefaultStream(&portaudio->stream,
                             0,           /* no input channels */
                             2,           /* stereo output */
                             paFloat32,   /* 32 bit floating point output */
                             44100,       /* sample rate*/
                             FIX2INT(framesPerBuffer),
                             paCallback,
                             (void*) portaudio);

  if (err != paNoError) {
    rb_raise(rb_eStandardError, "%s", Pa_GetErrorText(err));
  }

  self = Data_Wrap_Struct(rb_cPortaudio, 0, free_portaudio, portaudio);

  return self;
}


VALUE portaudio_wait(void *ptr)
{
  Portaudio *portaudio = (Portaudio *) ptr;

  pthread_cond_wait(&portaudio->cond, &portaudio->mutex);

  return Qnil;
}

float rms(float *v, int n)
{
  int i;
  float sum = 0.0;

  for (i = 0; i < n; i++) {
    sum += v[i] * v[i];
  }

  return sqrt(sum / n);
}

VALUE rb_portaudio_write_from_mpg(VALUE self, VALUE mpg)
{
  int err;
  size_t done = 0;
  Portaudio *portaudio;
  mpg123_handle *mh = NULL;

  Data_Get_Struct(self, Portaudio, portaudio);
  Data_Get_Struct(mpg, mpg123_handle, mh);

  err = mpg123_read(mh, (unsigned char *) portaudio->buffer, portaudio->size * sizeof(float), &done);

  portaudio->rms = rms(portaudio->buffer, portaudio->size);

  switch (err) {
    case MPG123_OK: return ID2SYM(rb_intern("ok"));
    case MPG123_DONE: return ID2SYM(rb_intern("done"));
    case MPG123_NEED_MORE: return ID2SYM(rb_intern("need_more"));
  }

  rb_raise(rb_eStandardError, "%s", mpg123_plain_strerror(err));
}

VALUE rb_portaudio_wait(VALUE self)
{
  Portaudio *portaudio;
  Data_Get_Struct(self, Portaudio, portaudio);

#ifdef RUBY_UBF_IO
  rb_thread_blocking_region(portaudio_wait, portaudio, RUBY_UBF_IO, NULL);
#else
  portaudio_wait(portaudio);
#endif
  return self;
}

VALUE rb_portaudio_write(VALUE self, VALUE buffer)
{
  int i, len;
  Portaudio *portaudio;
  Data_Get_Struct(self, Portaudio, portaudio);

  Check_Type(buffer, T_ARRAY);

  len = RARRAY_LEN(buffer);

  if (len != portaudio->size) {
    rb_raise(rb_eStandardError, "array length does not match buffer size: %d != %d", len, portaudio->size);
  }

  for (i = 0; i < len; i++) {
    portaudio->buffer[i] = NUM2DBL(rb_ary_entry(buffer, i));
  }

  return self;
}

VALUE rb_portaudio_rms(VALUE self)
{
  Portaudio *portaudio;
  Data_Get_Struct(self, Portaudio, portaudio);
  return rb_float_new(portaudio->rms);
}

VALUE rb_portaudio_start(VALUE self)
{
  Portaudio *portaudio;
  Data_Get_Struct(self, Portaudio, portaudio);
  int err = Pa_StartStream(portaudio->stream);

  pthread_cond_broadcast(&portaudio->cond);

  if (err != paNoError) {
    rb_raise(rb_eStandardError, "%s", Pa_GetErrorText(err));
  }

  return self;
}

VALUE rb_portaudio_stop(VALUE self)
{
  Portaudio *portaudio;
  Data_Get_Struct(self, Portaudio, portaudio);
  int err = Pa_StopStream(portaudio->stream);

  pthread_cond_broadcast(&portaudio->cond);

  if (err != paNoError) {
    rb_raise(rb_eStandardError, "%s", Pa_GetErrorText(err));
  }

  return self;
}

void Init_portaudio(void) {
  int err = Pa_Initialize();

  if (err != paNoError) {
    rb_raise(rb_eStandardError, "%s", Pa_GetErrorText(err));
  }

  rb_cPortaudio = rb_define_class("Portaudio", rb_cObject);

  rb_define_singleton_method(rb_cPortaudio, "new", rb_portaudio_new, 1);
  rb_define_method(rb_cPortaudio, "wait", rb_portaudio_wait, 0);
  rb_define_method(rb_cPortaudio, "rms", rb_portaudio_rms, 0);
  rb_define_method(rb_cPortaudio, "write", rb_portaudio_write, 1);
  rb_define_method(rb_cPortaudio, "write_from_mpg", rb_portaudio_write_from_mpg, 1);
  rb_define_method(rb_cPortaudio, "start", rb_portaudio_start, 0);
  rb_define_method(rb_cPortaudio, "stop", rb_portaudio_stop, 0);
}
