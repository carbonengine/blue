import blue
import contextlib
import scheduler
import traceback
import weakref
import datetime
import logging

logger = logging.getLogger(__name__)


STACKLESS_TRACING_ENABLED_KEY = "stackless_tracing_enabled"

# Counter that gets incremented
# to generate tasklet ids.
tasklet_id = 0


# Store all tasklets here.
tasklets = weakref.WeakKeyDictionary()



def _noop(*args, **kwargs):
    pass


class TaskletExt(scheduler.tasklet):
    __slots__ = [
        "context", "localStorage", "storedContext", 'startTime',
        "endTime", 'runTime', 'tasklet_id', 'origin_traceback', 'highlighted',
        'method_name', 'module_name', 'file_name', 'line_number',
        'parent_callsite',

        # trace context slots
        'tracer',      # None, or a stackless_tracing.CloneableTracer
        'trace_id',    # OTEL trace context trace_id
        'parent_id',   # OTEL trace context span_id
        'sampled',     # OTEL trace context sampled
        # trace state slots
        'ingress_id',   # Monolith specific trace state
        'sample_rate',  # sample rate used for this trace
        'creation_datetime',  # datetime when tasklet was created
    ]

    @staticmethod
    def GetWrapper(method):
        if not callable(method):
            raise TypeError("TaskletExt::__new__ argument \"method\" must be callable.")

        def CallWrapper(*args, **kwds):
            current = scheduler.getcurrent()
            current.startTime = blue.os.GetWallclockTimeNow()
            oldtimer = blue.pyos.taskletTimer.EnterTasklet(current.context)
            with _tasklet_trace(current):
                # noinspection PyBroadException
                try:
                    # noinspection PyBroadException
                    try:
                        return method(*args, **kwds)
                    except TaskletExit as e:
                        pass
                    except SystemExit as e:
                        logger.info("system %s exiting with %r" % (scheduler.getcurrent(), e))
                    except Exception:
                        logger.exception("Unhandled exception in %r" % scheduler.getcurrent())
                    return None  # Don't allow uncaught exceptions upwards.
                except Exception:
                    # Problem in exception handling.  Use traceback module.
                    traceback.print_exc()
                finally:
                    blue.pyos.taskletTimer.ReturnFromTasklet(oldtimer)
                    current.endTime = blue.os.GetWallclockTimeNow()
        return CallWrapper

    def __init__(self, context, method=None, stackless_tracing_enabled=True):
        if method is not None:
            super().__init__(TaskletExt.GetWrapper(method))
        else:
            super().__init__(method)

    def __new__(cls, ctx, method=None, stackless_tracing_enabled=True):
        global tasklet_id
        tid = tasklet_id
        tasklet_id += 1

        if method:
            t = scheduler.tasklet.__new__(cls, cls.GetWrapper(method))
        else:
            t = scheduler.tasklet.__new__(cls)

        t.creation_datetime = datetime.datetime.utcnow()
        # Inherit the localStorage from calling task.
        c = scheduler.getcurrent()
        ls = getattr(c, "localStorage", None)
        if ls is None:
            t.localStorage = {}
        else:
            t.localStorage = dict(ls) #copy it

        t.storedContext = t.context = ctx

        t.method_name = getattr(method, "__name__", "unknown_method")
        t.module_name = getattr(method, "__module__", "unknown_module")
        try:
            t.file_name = method.__code__.co_filename
            t.line_number = method.__code__.co_firstlineno
        except AttributeError:
            t.file_name = 'unknown_file'
            t.line_number = 0
        parent_module_name = getattr(c, "module_name", "unknown_parent_module")
        parent_method_name = getattr(c, "method_name", "unknown_parent_method")
        t.parent_callsite = "{}.{}".format(parent_module_name, parent_method_name),

        if stackless_tracing_enabled:
            cls._copy_tracer_and_state(c, t)

        t.runTime = 0.0
        t.tasklet_id = tid
        t.highlighted = False
        tasklets[t] = True  # Create a weakref to this tasklet.
        return t

    @staticmethod
    def _copy_tracer_and_state(old, new):
        trace_context_slots = [
            'trace_id',    # OTEL trace context trace_id
            'parent_id',   # OTEL trace context span_id
            'sampled',     # OTEL trace context sampled
            'ingress_id',  # Monolith specific trace state
            'sample_rate'
        ]
        for trace_slot in trace_context_slots:
            setattr(
                new,
                trace_slot,
                getattr(old, trace_slot, None)
            )
        tracer = getattr(old, 'tracer', None)
        if tracer is not None:
            setattr(new, 'tracer', tracer.clone())

    def bind(self, callableObject):
        return scheduler.tasklet.bind(self, self.CallWrapper(callableObject))

    def __repr__(self):
        abps = [getattr(self, attr) for attr in ["alive", "blocked", "paused", "scheduled"]]
        abps = "".join(str(int(flag)) for flag in abps)
        return "<TaskletExt object at 0x%x, abps=%s, ctxt=%r>" % (id(self), abps, getattr(self, 'storedContext', None))

    def __reduce__(self):
        """we don't support pickling of tasklets.  Intead, just return a special repr of it, so that
        they can be marshaled over for debugging purposes.

        Note:
        Not sure if this has utility at this point as attempting to marshal.Save, then marshal.Load
        a TaskletExt currently results in the following error:
        RuntimeError: HACKER WARNING! object bluepy.* is blacklisted
        """
        return str, ("__reduce__()'d "+repr(self),)

    def PushTimer(self, ctxt):
        blue.pyos.taskletTimer.EnterTasklet(ctxt)

    def PopTimer(self, ctxt):
        blue.pyos.taskletTimer.ReturnFromTasklet(ctxt)

    def GetCurrent(self):
        return blue.pyos.taskletTimer.GetCurrent()

    def GetWallclockTime(self):
        """Return the wallclock time in seconds since this tasklet was started"""
        try:
            return (blue.os.GetWallclockTimeNow() - self.startTime) * 1e-7
        except AttributeError:
            return None

    def GetRunTime(self):
        """Return the accumulated run time in seconds of this tasklet"""
        if hasattr(self, "startTime"):
            return self.runTime + blue.pyos.GetTimeSinceSwitch()
        return 0.0


def _tasklet_trace(t):
    tracer = getattr(t, 'tracer', None)
    if tracer is None:
        return _no_tasklet_tracer()

    return tracer.get_tasklet_tracer(t)


@contextlib.contextmanager
def _no_tasklet_tracer():
    yield
    return


def _tracing_enabled(**kwargs):
    if STACKLESS_TRACING_ENABLED_KEY not in list(kwargs.keys()):
        return True, kwargs

    stackless_tracing_enabled = kwargs[STACKLESS_TRACING_ENABLED_KEY]
    del kwargs[STACKLESS_TRACING_ENABLED_KEY]
    return stackless_tracing_enabled, kwargs
