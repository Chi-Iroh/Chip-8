#pragma once

#include "pixel.hpp"

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif

#include <array>
#include <bitset>
#include <string_view>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <SFML/Audio.hpp>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

class CPU {
public:
	using byte_t = unsigned char;
	using address_t = short unsigned;

	static constexpr address_t memorySize{ 0x1000 };
	static constexpr address_t memoryStart{ 0x200 };
	static constexpr address_t memoryUsableSize{ memorySize - memoryStart };

	static constexpr std::size_t fontSize{ 5 };

private:
	sf::RenderWindow window{ sf::VideoMode(Screen::width, Screen::height), "" };

	Screen							screen{};

	std::array<byte_t, memorySize>	memory{};	// memory to store ROM
	std::array<byte_t, 0x10>			 V{};	// register
	std::stack<address_t>			 jumps{};	// contains addresses of subroutines calls

	// emulator => Chip 8
	// 1 2 3 4	=> 1 2 3 C
	// A Z E R	=> 4 5 6 D
	// Q S D F	=> 7 8 9 E
	// W X C V	=> A 0 B F

	using key_t = std::pair<sf::Keyboard::Key /* real input : emulator */, sf::Keyboard::Key /* virtual key : Chip8 */>;

	enum class Chip8Key {
		_1 = sf::Keyboard::Num1,
		_2 = sf::Keyboard::Num2,
		_3 = sf::Keyboard::Num3,
		_4 = sf::Keyboard::A,
		_5 = sf::Keyboard::Z,
		_6 = sf::Keyboard::E,
		_7 = sf::Keyboard::Q,
		_8 = sf::Keyboard::S,
		_9 = sf::Keyboard::D,
		_0 = sf::Keyboard::X,
		_A = sf::Keyboard::W,
		_B = sf::Keyboard::C,
		_C = sf::Keyboard::Num4,
		_D = sf::Keyboard::R,
		_E = sf::Keyboard::F,
		_F = sf::Keyboard::V
	};

	std::unordered_map<Chip8Key, bool /*state */> keys{
		{Chip8Key::_0, false},
		{Chip8Key::_1, false},
		{Chip8Key::_2, false},
		{Chip8Key::_3, false},
		{Chip8Key::_4, false},
		{Chip8Key::_5, false},
		{Chip8Key::_6, false},
		{Chip8Key::_7, false},
		{Chip8Key::_8, false},
		{Chip8Key::_9, false},
		{Chip8Key::_A, false},
		{Chip8Key::_B, false},
		{Chip8Key::_C, false},
		{Chip8Key::_D, false},
		{Chip8Key::_E, false},
		{Chip8Key::_F, false},
	};

	address_t							 I{};	// the "address register" -> stores an address	
	address_t			   pc{ memoryStart };	// program counter : to iterate over the memory
	
	static constexpr std::size_t pcIncrement{ sizeof(address_t) / sizeof(byte_t) };

	byte_t							nJumps{};			// current number of jumps (subroutines calls), must be < 16
	byte_t							gameTimer{};
	byte_t							soundTimer{};

	using opcodeHex_t = address_t;
	static constexpr std::size_t nOpcodes{ 35 };
	std::string ROM_{};

	enum class Opcode { // underscores to avoid naming problems (an identifier cannot begin with a number)
		// calls RCA 1082's routine at address NNN (not used in modern implementations)
		_0NNN = 0x0000,

		// erases screen
		_00E0 = 0x00E0,

		// returns from a subroutine
		_00EE = 0x00EE,

		// jumps to address NNN
		_1NNN = 0x1000,

		// calls subroutine at address NNN
		_2NNN = 0x2000,

		// skips next instruction if VX == NN
		_3XNN = 0x3000,

		// skips next instruction if VX != NN
		_4XNN = 0x4000,

		// skips next instrucion if VX == VY
		_5XY0 = 0x5000,

		// VX = NN
		_6XNN = 0x6000,

		// VX += NN
		_7XNN = 0x7000,

		// VX = VY
		_8XY0 = 0x8000,

		// VX |= VY
		_8XY1 = 0x8001,

		// VX &= VY
		_8XY2 = 0x8002,

		// VX ^= VY
		_8XY3 = 0x8003,

		// VX += VY; VF is set to 1 if overflow (carry), otherwise to 0
		_8XY4 = 0x8004,

		// VX -= VY; VF is set to 0 if overflow (borrow); otherwise to 1
		_8XY5 = 0x8005,

		// VX >>= 1; VF = VX's LSB before shift
		_8XY6 = 0x8006,

		// VX = VY - VX; VF is set to 0 if overflow (borrow), otherwise to 1
		_8XY7 = 0x8007,

		// VX <<= 1; VF is set to VX's MSB before shift
		_8XYE = 0x800E,

		// skips next instruction if VX != VY
		_9XY0 = 0x9000,

		// I = NNN
		_ANNN = 0xA000,

		// jumps to address V0 + NNN
		_BNNN = 0xB000,

		// VX = random number lesser than NN
		_CXNN = 0xC000,

		// displays a sprite at coords (VX ; VY), the sprite has a 8px width and a Npx height.
		// each row of 8 pixels is read as binary-coded from memory[I], I isn't modified
		// VF is set to 1 is collision, otherwise 0
		_DXYN = 0xD000,

		// skips next instruction if key stored in VX's pressed
		_EX9E = 0xE09E,

		// skips next instruction if key stored in VX isn't pressed
		_EXA1 = 0xE0A1,

		// VX = gameTimer
		_FX07 = 0xF007,

		// waits for a key press, then store it into VX
		_FX0A = 0xF00A,

		// gameTimer = VX
		_FX15 = 0xF015,

		// soundTimer = VX
		_FX18 = 0xF018,

		// I += VX; VF is set to 1 if overflow (carry), otherwise to 0
		_FX1E = 0xF01E,

		// I = address of character stored in VX; characters 0-H (in hex) are represented by a 4*5 font
		_FX29 = 0xF029,

		// copies VX's decimal value in memory, at addresses I (hundreds), I+1 (tens), and I+2 (units)
		_FX33 = 0xF033,

		// stores V0 to VX (included) in memory starting at address I (isn't modified)
		_FX55 = 0xF055,

		// fills V0 to VX (included) from memory starting at address I (isn't modified)
		_FX65 = 0xF065
	};

	using mask_t = opcodeHex_t;
	using results_t = std::pair<mask_t, Opcode>;

	static constexpr std::array<results_t, nOpcodes> opcodesAND{
		results_t{0xF000, Opcode::_0NNN},
		results_t{0xFFFF, Opcode::_00E0},
		results_t{0xFFFF, Opcode::_00EE},
		results_t{0xF000, Opcode::_1NNN},
		results_t{0xF000, Opcode::_2NNN},
		results_t{0xF000, Opcode::_3XNN},
		results_t{0xF000, Opcode::_4XNN},
		results_t{0xF00F, Opcode::_5XY0},
		results_t{0xF000, Opcode::_6XNN},
		results_t{0xF000, Opcode::_7XNN},
		results_t{0xF00F, Opcode::_8XY0},
		results_t{0xF00F, Opcode::_8XY1},
		results_t{0xF00F, Opcode::_8XY2},
		results_t{0xF00F, Opcode::_8XY3},
		results_t{0xF00F, Opcode::_8XY4},
		results_t{0xF00F, Opcode::_8XY5},
		results_t{0xF00F, Opcode::_8XY6},
		results_t{0xF00F, Opcode::_8XY7},
		results_t{0xF00F, Opcode::_8XYE},
		results_t{0xF00F, Opcode::_9XY0},
		results_t{0xF000, Opcode::_ANNN},
		results_t{0xF000, Opcode::_BNNN},
		results_t{0xF000, Opcode::_CXNN},
		results_t{0xF000, Opcode::_DXYN},
		results_t{0xF0FF, Opcode::_EX9E},
		results_t{0xF0FF, Opcode::_EXA1},
		results_t{0xF0FF, Opcode::_FX07},
		results_t{0xF0FF, Opcode::_FX0A},
		results_t{0xF0FF, Opcode::_FX15},
		results_t{0xF0FF, Opcode::_FX18},
		results_t{0xF0FF, Opcode::_FX1E},
		results_t{0xF0FF, Opcode::_FX29},
		results_t{0xF0FF, Opcode::_FX33},
		results_t{0xF0FF, Opcode::_FX55},
		results_t{0xF0FF, Opcode::_FX65},
	};

	void checkKeys();

	static std::string opcodeToStr(opcodeHex_t opcode, std::size_t finalLength = 4);

	static constexpr bool isOpcode(opcodeHex_t opcode) noexcept;

	void initializeFonts();

	void count() noexcept;

	opcodeHex_t nextOpcode() const;

	void interpretOpcode(opcodeHex_t opcode);

	bool loadGame(const std::string& ROM);

	static Chip8Key byteToChip8Key(byte_t keycode);

	static char chip8KeyName(sf::Keyboard::Key key) noexcept;

	// if there's any opcode to execute after this one
	constexpr bool isThereOpcodeAfter() const noexcept;

public:
	static constexpr auto FPS{ 60u };
	static constexpr auto delay{ 1.f / FPS }; // delay time in seconds; should be float to avoid truncating while calling sf::seconds

	static constexpr auto frequency{ 250u };
	static constexpr auto opcodesPerFrame{ 1000u / frequency }; // while <delay> ms, <opcodesPerSecond> operations must be done

	CPU();

	CPU(const CPU&)				= delete;
	CPU(CPU&&)					= delete;

	CPU& operator=(const CPU&)  = delete;
	CPU& operator=(CPU&&)		= delete;

	void emulate(const std::string& ROM);

	static address_t randomNumber(address_t max) noexcept;
};