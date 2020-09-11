#include <inttypes.h>
#include <cstdio>
#include <vector>
#include <utility>
#include <memory>
#include <string>
#include <map>

#include <algorithm>

#include <iostream>
#include <fstream>
#include <streambuf>

class StdOutputter
{
public:
    void out(uint8_t x) const
    {
        std::putchar(x);
    }
};

class HexOutputter
{
public:
    void out(uint8_t x) const
    {
        std::printf("%02x", x & 0xff);
    }
};

class StdInputter
{
public:
    uint8_t in() const
    {
        return std::getchar();
    }
};

template <
    class DataCounterPolicy = size_t,
    class ProgramCounterPolicy = size_t,
    class FieldPolicy = uint8_t *>
struct BrainfuckState
{
public:
    DataCounterPolicy data_counter;
    ProgramCounterPolicy program_counter;
    FieldPolicy field;
};

template <class BFState = BrainfuckState<>>
class Instruction
{
public:
    virtual void execute(BFState &) const = 0;
    virtual ~Instruction() = default;
};

template <class BFState = BrainfuckState<>>
class NextDataInstruction : public Instruction<BFState>
{
public:
    virtual void execute(BFState &state) const final
    {
        state.data_counter++;
    }
};

template <class BFState = BrainfuckState<>>
class PrevDataInstruction : public Instruction<BFState>
{
public:
    virtual void execute(BFState &state) const final
    {
        state.data_counter--;
    }
};

template <class BFState = BrainfuckState<>>
class IncDataInstruction : public Instruction<BFState>
{
public:
    virtual void execute(BFState &state) const final
    {
        state.field[state.data_counter]++;
    }
};

template <class BFState = BrainfuckState<>>
class DecDataInstruction : public Instruction<BFState>
{
public:
    virtual void execute(BFState &state) const final
    {
        state.field[state.data_counter]--;
    }
};

template <class BFState = BrainfuckState<>>
class JumpZeroInstruction : public Instruction<BFState>
{
public:
    JumpZeroInstruction(size_t to)
        : m_to(to)
    {
    }

    virtual void execute(BFState &state) const final
    {
        if (0 == state.field[state.data_counter])
        {
            state.program_counter = m_to;
        }
    }

private:
    size_t m_to;
};

template <class BFState = BrainfuckState<>>
class JumpNonzeroInstruction : public Instruction<BFState>
{
public:
    JumpNonzeroInstruction(size_t to)
        : m_to(to)
    {
    }

    virtual void execute(BFState &state) const final
    {
        if (0 != state.field[state.data_counter])
        {
            state.program_counter = m_to;
        }
    }

private:
    size_t m_to;
};

template <
    class BFState = BrainfuckState<>,
    class Inputter = StdInputter>
class InInstruction : public Instruction<BFState>, private Inputter
{
public:
    virtual void execute(BFState &state) const final
    {
        state.field[state.data_counter] = this->in();
    }
};

template <
    class BFState = BrainfuckState<>,
    class Outputter = StdOutputter>
class OutInstruction : public Instruction<BFState>, private Outputter
{
public:
    virtual void execute(BFState &state) const final
    {
        this->out(state.field[state.data_counter]);
    }
};

template <
    class BFCode,
    class BFState = BrainfuckState<>>
class BrainfuckInterpreter
{
public:
    BrainfuckInterpreter(BFCode code)
        : m_code(std::move(code))
    {
    }

    void step(BFState &state)
    {
        m_code[state.program_counter]->execute(state);
        state.program_counter++;
    }

    void interpret(BFState &state)
    {
        while (state.program_counter < m_code.size())
        {
            step(state);
        }
    }

private:
    BFCode m_code;
};

template <class BFState>
using unique_ptr_code = std::vector<std::unique_ptr<Instruction<BFState>>>;

template <class BFState>
unique_ptr_code<BFState> parse_code(const std::string &code)
{
    unique_ptr_code<BFState> ret(code.length());
    std::vector<size_t> brackets;

    for (size_t i = 0; i < code.length(); ++i)
    {
        switch (code[i])
        {
        case '+':
            ret[i] = std::make_unique<IncDataInstruction<BFState>>();
            break;
        case '-':
            ret[i] = std::make_unique<DecDataInstruction<BFState>>();
            break;
        case '>':
            ret[i] = std::make_unique<NextDataInstruction<BFState>>();
            break;
        case '<':
            ret[i] = std::make_unique<PrevDataInstruction<BFState>>();
            break;
        case '.':
            ret[i] = std::make_unique<OutInstruction<BFState, StdOutputter>>();
            break;
        case ',':
            ret[i] = std::make_unique<InInstruction<BFState>>();
            break;
        case '[':
            brackets.push_back(i);
            break;
        case ']':
            size_t j = brackets.back();
            brackets.pop_back();

            ret[j] = std::make_unique<JumpZeroInstruction<BFState>>(i);
            ret[i] = std::make_unique<JumpNonzeroInstruction<BFState>>(j);
            break;
        }
    }
    return ret;
}

template <
    class BFState,
    class Outputter = StdOutputter,
    class Inputter = StdInputter>
class FlyweightCode
{
public:
    template <typename StringT>
    FlyweightCode(StringT code) : m_code(code)
    {
        build_bracket_map();
    }

    const Instruction<BFState> *operator[](size_t i) const
    {
        switch (m_code[i])
        {
        case '+':
            return &m_inc;
            break;
        case '-':
            return &m_dec;
            break;
        case '>':
            return &m_next;
            break;
        case '<':
            return &m_prev;
            break;
        case '.':
            return &m_out;
            break;
        case ',':
            return &m_in;
            break;
        case '[':
        case ']':
            return m_bracket_map.at(i).get();
            break;
        }
        return nullptr;
    }

    size_t size() const
    {
        return m_code.length();
    }

private:
    void build_bracket_map()
    {
        std::vector<size_t> bracket_stack;

        for (size_t i = 0; i < m_code.length(); ++i)
        {
            if ('[' == m_code[i])
            {
                bracket_stack.push_back(i);
            }
            else if (']' == m_code[i])
            {
                size_t j = bracket_stack.back();
                bracket_stack.pop_back();

                m_bracket_map[j] = std::make_unique<JumpZeroInstruction<BFState>>(i);
                m_bracket_map[i] = std::make_unique<JumpNonzeroInstruction<BFState>>(j);
            }
        }
    }

    std::string m_code;
    std::map<size_t, std::unique_ptr<Instruction<BFState>>> m_bracket_map;

    IncDataInstruction<BFState> m_inc;
    DecDataInstruction<BFState> m_dec;
    NextDataInstruction<BFState> m_next;
    PrevDataInstruction<BFState> m_prev;
    InInstruction<BFState, Inputter> m_in;
    OutInstruction<BFState, Outputter> m_out;
};

inline bool is_bf_char(char c)
{
    switch (c)
    {
    case '+':
    case '-':
    case '>':
    case '<':
    case '.':
    case ',':
    case '[':
    case ']':
        return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " [bf-file]" << std::endl;
        return 1;
    }

    std::ifstream source_stream(argv[1]);

    // This could be optimized by reserving...
    std::string code_string;

    std::copy_if(
        (std::istreambuf_iterator<char>(source_stream)),
        std::istreambuf_iterator<char>(),
        std::back_inserter(code_string),
        is_bf_char);

    BrainfuckState<size_t, size_t, std::unique_ptr<uint8_t[]>> state{
        0ull,
        0ull,
        std::make_unique<uint8_t[]>(0x2000),
    };
    std::memset(state.field.get(), 0, 0x2000);

    auto code = FlyweightCode<decltype(state)>(std::move(code_string));

    BrainfuckInterpreter<decltype(code), decltype(state)> interpreter(std::move(code));

    interpreter.interpret(state);
}
