#include <ruby.h>
#include <pthread.h>
#include <portaudio.h>

typedef struct {
  PaStream *stream;
  int size;
  float *buffer;
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

  for (i = 0; i < framesPerBuffer * 2; i++) {
    *out++ = portaudio->buffer[i];
  }

  pthread_cond_broadcast(&portaudio->cond);
  pthread_mutex_unlock(&portaudio->mutex);

  return 0;
}

VALUE rb_portaudio_new(VALUE klass)
{
  PaStreamParameters outputParameters;
  const PaDeviceInfo *deviceInfo;
  PaError err;
  int device, numDevices;
  VALUE self;
  Portaudio *portaudio = (Portaudio *) malloc(sizeof(Portaudio));

  pthread_mutex_init(&portaudio->mutex, NULL);
  pthread_cond_init(&portaudio->cond, NULL);

  portaudio->size = 4096 * 2;
  portaudio->buffer = (float *) malloc(sizeof(float) * portaudio->size);

  err = Pa_OpenDefaultStream(&portaudio->stream,
                             0,           /* no input channels */
                             2,           /* stereo output */
                             paFloat32,   /* 32 bit floating point output */
                             44100,       /* 44100 sample rate*/
                             4096,        /* frames per buffer */
                             paCallback,  /* this is your callback function */
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

VALUE rb_portaudio_write(VALUE self, VALUE buffer)
{
  int i;
  Portaudio *portaudio;
  Data_Get_Struct(self, Portaudio, portaudio);

  Check_Type(buffer, T_ARRAY);

  for (i = 0; i < portaudio->size; i++) {
    portaudio->buffer[i] = NUM2DBL(rb_ary_entry(buffer, i));
  }

#ifdef RUBY_UBF_IO
  rb_thread_blocking_region(portaudio_wait, portaudio, RUBY_UBF_IO, NULL);
#else
  portaudio_wait(portaudio);
#endif

  return self;
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

  rb_define_singleton_method(rb_cPortaudio, "new", rb_portaudio_new, 0);
  rb_define_method(rb_cPortaudio, "write", rb_portaudio_write, 1);
  rb_define_method(rb_cPortaudio, "start", rb_portaudio_start, 0);
  rb_define_method(rb_cPortaudio, "stop", rb_portaudio_stop, 0);
}
