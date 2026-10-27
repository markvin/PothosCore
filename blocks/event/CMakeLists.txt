########################################################################
# EventBlocks module
########################################################################
pothos_blocks_module(
    TARGET EventBlocks
    SOURCES
        PeriodicTrigger.cpp
        TriggeredSignal.cpp
        TestPeriodicTrigger.cpp
        MessageToSignal.cpp
        SlotToMessage.cpp
        LabelToMessage.cpp
        TestSignalsAndSlots.cpp
        Evaluator.cpp
        MessagePrinter.cpp
    TESTS
        test_periodic_trigger
        test_signals_and_slots
        test_evaluator
        test_evaluator_multiarg
        test_evaluator_multislot
)
