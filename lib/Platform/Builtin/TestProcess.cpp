// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Exception.hpp>
#include <Pothos/System/Paths.hpp>
#include "Platform/Process.hpp"
#include "Platform/Environment.hpp"
#include <iostream>
#include <string>
#include <vector>

using namespace Pothos::Platform;

POTHOS_TEST_BLOCK("/platform/tests", test_windows_command_line_quoting)
{
    const auto cmd = buildWindowsCommandLine(
        "C:\\Program Files\\P\\PothosUtil.exe",
        {"--load-module", "C:\\my mods\\a.dll", "say \"hi\"", "trail\\", ""});
    POTHOS_TEST_TRUE(cmd == L"\"C:\\Program Files\\P\\PothosUtil.exe\" --load-module \"C:\\my mods\\a.dll\" \"say \\\"hi\\\"\" trail\\ \"\"");
}

POTHOS_TEST_BLOCK("/platform/tests", test_termination_event_name)
{
    //the named event of Poco::Util::ServerApplication: POCOTRM + 8 upper case hex digits
    POTHOS_TEST_TRUE(terminationEventName(0xABCD) == L"POCOTRM0000ABCD");
    POTHOS_TEST_TRUE(terminationEventName(1234) == L"POCOTRM000004D2");
}

POTHOS_TEST_BLOCK("/platform/tests", test_pipe_roundtrip)
{
    Pipe p;
    {
        PipeOutputStream os(p);
        os << "hello\nworld\n" << std::flush;
    }
    p.closeWrite();
    PipeInputStream is(p);
    std::string line;
    POTHOS_TEST_TRUE(bool(std::getline(is, line)));
    POTHOS_TEST_EQUAL(line, "hello");
    POTHOS_TEST_TRUE(bool(std::getline(is, line)));
    POTHOS_TEST_EQUAL(line, "world");
    POTHOS_TEST_TRUE(not std::getline(is, line));
}

POTHOS_TEST_BLOCK("/platform/tests", test_process_exit_code)
{
    ProcessOptions opts;
    opts.args = {"--success-code", "7"};
    auto proc = Process::launch(Pothos::System::getPothosUtilExecutablePath(), opts);
    POTHOS_TEST_TRUE(proc.id() > 0);
    POTHOS_TEST_EQUAL(proc.wait(), 7);
}

POTHOS_TEST_BLOCK("/platform/tests", test_process_capture_stdout)
{
    Pipe out;
    ProcessOptions opts;
    opts.args = {"--help"};
    opts.out = &out;
    auto proc = Process::launch(Pothos::System::getPothosUtilExecutablePath(), opts);
    PipeInputStream is(out);
    const std::string text((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    POTHOS_TEST_EQUAL(proc.wait(), 0);
    POTHOS_TEST_TRUE(text.find("self-tests") != std::string::npos);
}

//helper for test_process_large_output: prints only when asked through the environment
POTHOS_TEST_BLOCK("/platform/tests", helper_large_output)
{
    if (getEnv("POTHOS_TEST_LARGE_OUTPUT") != std::optional<std::string>("1")) return;
    const std::string line(63, 'x');
    for (int i = 0; i < 4096; i++) std::cout << line << '\n';
    std::cout << std::flush;
}

POTHOS_TEST_BLOCK("/platform/tests", test_process_large_output)
{
    Pipe out;
    ProcessOptions opts;
    opts.args = {"--self-test1", "/platform/tests/helper_large_output"};
    opts.env = {{"POTHOS_TEST_LARGE_OUTPUT", "1"}};
    opts.out = &out;
    auto proc = Process::launch(Pothos::System::getPothosUtilExecutablePath(), opts);
    PipeInputStream is(out);
    const std::string xs(63, 'x');
    size_t count = 0;
    for (std::string line; std::getline(is, line);)
    {
        if (not line.empty() and line.back() == '\r') line.pop_back();
        if (line == xs) count++;
    }
    POTHOS_TEST_EQUAL(proc.wait(), 0);
    POTHOS_TEST_EQUAL(count, 4096u);
}

POTHOS_TEST_BLOCK("/platform/tests", test_process_launch_failure)
{
    POTHOS_TEST_THROWS(Process::launch("/no/such/exe", ProcessOptions()), Pothos::SystemException);
}
