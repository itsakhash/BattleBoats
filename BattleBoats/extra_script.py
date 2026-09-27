"""
extra_script.py

Dependency importer, extraordinaire.

@date   25 Nov 2024
"""
Import("env")

def add_object_files(env, target=None, source=None):
    env.Append(LINKFLAGS=[
        "./lib/objs/Field_correct.o",
        "./lib/objs/HumanAgent.o",
        "./lib/objs/Message_correct.o",
        "./lib/objs/Negotiation_correct.o"
    ])

env.AddPreAction("buildprog", add_object_files)
