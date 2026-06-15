# Copyright © 2017 CCP ehf.

from setuptools import setup, Distribution
import os
import shutil
import imp


lib_path = os.path.join(os.getcwd(), 'blue.pyd')
blue = imp.load_dynamic('blue', lib_path)


class BinaryDistribution(Distribution):
    def has_ext_modules(foo):
        return True


def getVersion():
    return "{}+{}".format(blue.GetChangelist(), blue.GetBranch().lower())


# Report the blue version out to Teamcity
print("##teamcity[setParameter name='env.BLUE_VERSION' value='%s']" % getVersion())


os.chdir(os.path.dirname(__file__))
shutil.copyfile(lib_path, os.path.join(os.getcwd(), 'blue', 'blue.pyd'))

setup(
    name="blue",
    version=getVersion(),
    description='Blue Library',
    packages=['blue'],
    package_data={
        'blue': ['blue.pyd'],
    },
    distclass=BinaryDistribution,
)
