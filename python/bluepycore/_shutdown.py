# Copyright © 2023 CCP ehf.

import logging
import scheduler
import traceback

from ._taskletextension import TaskletExt, tasklets


logger = logging.getLogger(__name__)


def Shutdown(exitprocs):
    """
    Called from the main tasklet in bluepython.cpp to
    * 1 run all exit procs
    * 2 dump alive tasklets
    * 3 kill alive tasklets
    * 4 dumpdalive tasklets again
    """
    # 1 run exitprocs.  Must happen on a taskletExt
    def RunAll():
        scheduler.getcurrent().block_trap = True
        for proc in exitprocs:
            try:
                proc()
            except Exception:
                logger.exception("exitproc " + repr(proc))

    if exitprocs:
        TaskletExt('Shutdown', RunAll)()
        intr = scheduler.run(1000000)
        if intr:
            logger.error("ExitProcs interrupted at tasklet "+ repr(intr))

    # 2 dump alive tasklets
    _GetTaskletDump(True)
    if len(tasklets):
        # 3
        _KillTasklets()
        # 4
        _GetTaskletDump(True)


def _GetTaskletDump(logIt=True):
    """
    This is called at shutdown
    to list the currently alive tasklets.
    """
    loglines = []
    loglines.append("GetTaskletDump:  %s TaskletExt objects alive" % len(tasklets))
    loglines.append("[context] - [code] - [stack depth] - [creation context]")
    for t in tasklets.keys():
        try:
            if t.frame:
                stack = traceback.extract_stack(t.frame, 1)
                depth = len(stack)
                f = stack[-1]
                code = "%s(%s)" % (f[0], f[1])
            else:
                code, depth = "<no frame>", 0
        except Exception as e:
            code, depth = repr(e), 0

        ctx = getattr(t, "context", "(unknown)"),
        sctx = getattr(t, "storedContext", "(unknown)")
        line = "%s - %s - %s - %s" % (sctx, code, depth, ctx)
        loglines.append(line)
    loglines.append("End TaskletDump")
    if (logIt):
        for line in loglines:
            logger.info(line)
    return "\n".join(loglines)+"\n"


def _KillTasklets():
    """
    TODO: This is leftover from our migration away from stackless and should be verified whether it's still required.
    """

    """
    This must happen in a tasklet, otherwise, Stackless freaks out!
    """
    t = TaskletExt("KillTasklets", _KillTasklets_)
    t()
    t.run() #run it now


def _KillTasklets_():
    logger.info("killing all %s TaskletExt objects" % len(tasklets))
    # Do this when shutting down
    for i in range(3):
        # try three times, sometimes killing a tasklet spawns a new one!
        for tasklet in tasklets.keys():
            if tasklet is scheduler.getcurrent():
                continue
            try:
                if tasklet.frame:
                    logger.info("killing %s" % tasklet)
                    tasklet.kill()
                else:
                    logger.info("ignoring %r, no frame." % tasklet)
            except RuntimeError as e:
                logger.warn("couldn't kill %r: %r" % (tasklet, e))
    logger.info("killing done")
