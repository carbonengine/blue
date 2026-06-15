# Copyright © 2023 CCP ehf.

import blue
import contextlib
import scheduler
import traceback
import weakref
import datetime
import logging

from scheduler import TaskletExit

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
    _exception_handler = logger.exception
    __slots__ = [
        "localStorage", "storedContext",
         'tasklet_id', 'origin_traceback',
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

    def __init__(self, context, method=None, stackless_tracing_enabled=True):
        super().__init__()
        self.bind(method)

        c = scheduler.getcurrent()

        self.storedContext = self.context = context

        parent_module_name = getattr(c, "module_name", "unknown_parent_module")
        parent_method_name = getattr(c, "method_name", "unknown_parent_method")
        self.parent_callsite = "{}.{}".format(parent_module_name, parent_method_name)
        self.runTime = 0.0
        self.highlighted = False


    def __new__(cls, ctx, method=None, stackless_tracing_enabled=True):
        global tasklet_id
        tid = tasklet_id
        tasklet_id += 1

        t = scheduler.tasklet.__new__(cls)

        t.creation_datetime = datetime.datetime.now(datetime.UTC)
        # Inherit the localStorage from calling task.
        c = scheduler.getcurrent()
        ls = getattr(c, "localStorage", None)
        if ls is None:
            t.localStorage = {}
        else:
            t.localStorage = dict(ls) #copy it

        if stackless_tracing_enabled:
            cls._copy_tracer_and_state(c, t)

        t.tasklet_id = tid
        tasklets[t] = True  # Create a weakref to this tasklet.
        return t

    @staticmethod
    def HandleException(infoString):
        if TaskletExt._exception_handler:
            TaskletExt._exception_handler(infoString)

    @staticmethod
    def SetExceptionHandler(callback):
        """
        Set the exception handler callback for when
        an exception is raised within the tasklet.
        The callback should accept one parameter
        of type 'str'.

        :params callback: The callback to call when an exception occurs.
        :type callback: callable
        """
        if not callable(callback):
            raise TypeError("Callback not callable")
        TaskletExt._exception_handler = callback

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
        self.dont_raise = True
        self.context_manager_getter = _tasklet_trace
        self.exception_handler = TaskletExt.HandleException
        return scheduler.tasklet.bind(self, callableObject)

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
        if self.startTime == 0.0:
            return None
        else:
            return (blue.os.GetWallclockTimeNow() - self.startTime) * 1e-7

    def GetRunTime(self):
        """Return the accumulated run time in seconds of this tasklet"""
        if self.startTime == 0.0:
            return 0.0
        else:
            return self.runTime  + blue.pyos.GetTimeSinceSwitch()

class WrapperTaskletContextManager:
    def __init__(self, tasklet, *args):
        self.tasklet = tasklet
        self.oldTime = None
        self.managers = []
        for c in args:
            self.managers.append(c)

    def __enter__(self):
        self.oldTime = blue.pyos.taskletTimer.EnterTasklet(self.tasklet.context)
        for manager in self.managers:
            manager.__enter__()

    def __exit__(self, *args):
        blue.pyos.taskletTimer.ReturnFromTasklet(self.oldTime)
        for manager in self.managers:
            manager.__exit__(*args)

def _tasklet_trace(t):
    tracer = getattr(t, 'tracer', None)
    if tracer is None:
        return WrapperTaskletContextManager(t)

    return WrapperTaskletContextManager(t, tracer.get_tasklet_tracer(t))

def _tracing_enabled(**kwargs):
    if STACKLESS_TRACING_ENABLED_KEY not in list(kwargs.keys()):
        return True, kwargs

    stackless_tracing_enabled = kwargs[STACKLESS_TRACING_ENABLED_KEY]
    del kwargs[STACKLESS_TRACING_ENABLED_KEY]
    return stackless_tracing_enabled, kwargs
