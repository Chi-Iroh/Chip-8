#include "cpu.hpp"

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif

#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <random>
#include <chrono>
#include <bitset>
#include <sstream>
#include <vector>
#include <filesystem>
#include <codecvt>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

constexpr std::string toString(const std::wstring& wstr) {
	std::string result{};
	result.reserve(wstr.size());
	for (wchar_t wc : wstr) {
		result += static_cast<char>(wc);
	}
	return result;
}

#define ROM_NAME(ROM)								toString(std::filesystem::path(ROM).filename().native())

#define MSG_BASE(msg, code, ostream)				ostream << std::string(msg); if (std::string(code) != "") { ostream << " : " << (code); } ostream << " !" << std::endl

#define WARNING(msg, code)							std::cout << "Warning "; MSG_BASE(msg, code, std::cout)
#define ERROR(msg, code)							std::cerr << "Error "; MSG_BASE(msg, code, std::cerr)

#define END_PROGRAM_MSG(msg, ROMname)				MSG_BASE("-- End of Emulation (" + (ROMname) + ") -- " + (msg), "", std::cout)
#define END_PROGRAM_ERR(msg, ROMname, errorCode)	MSG_BASE("-- End of Emulation (" + (ROMname) + ") -- " + (msg), std::string(errorCode), std::cerr)

#define QUIT_IF_NOTHING_TO_EMULATE    if (pc == memorySize || !isThereOpcodeAfter()) {END_PROGRAM_MSG("-- End of Program -- Emulation successfully ended !", ROMname); window.close(); break; break;}

#ifndef NDEBUG
#define ASSERT_MSG(expression, msg, errorCode) if (!(expression)) { ERROR("Assertion failed at line " + std::to_string(__LINE__) + " : " + std::string(msg), std::string(errorCode) == "" ? "ASSERTION_FAIL" : (errorCode)); std::terminate(); }
#else
#define ASSERT_MSG(expression, msg, errorCode) {}
#endif

// static cast to address type
#define UCAST(expr) static_cast<address_t>((expr))

// static cast to byte type
#define BCAST(expr) static_cast<byte_t>((expr))

#ifndef NDEBUG
#define DEBUG_BASE(msg, ostream) ostream << "Opcode " << opcodeToStr(opcode) << " : " << (msg) << std::endl << std::endl
#define DEBUG_CONSOLE(msg) DEBUG_BASE(msg, std::cout)

#define DEBUG_FILE(msg)														\
{																			\
	std::ofstream log{ "results.log", std::ios::app };						\
	if (!log) {																\
		std::cerr << "Cannot create/open file results.log !" << std::endl;	\
	}																		\
	else {																	\
		DEBUG_BASE(msg, log);												\
	}																		\
}

#define DEBUG(msg) DEBUG_FILE(msg); DEBUG_CONSOLE(msg)
#else
#define DEBUG(msg)
#endif

#define PAUSE_IF_NOT_FOCUS(title) while (event.type == sf::Event::LostFocus) {window.setTitle("[Paused] -- " + title); while (event.type != sf::Event::GainedFocus) {window.pollEvent(event);} window.setTitle(title); break;}

CPU::address_t CPU::randomNumber(address_t max) noexcept {
	static std::random_device device{};
#define NOW_TIME std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count()

	static const auto firstCallTime{ NOW_TIME };
	std::mt19937 generator{ static_cast<std::mt19937::result_type>(NOW_TIME - firstCallTime) };
	std::uniform_int_distribution<address_t> distribution{ 0u, max };
	return std::abs((distribution(generator) * distribution(generator)) ^ distribution(generator)) % (max + 1u); // result must be <= than max

#undef NOW_TIME
}

void CPU::count() noexcept{
	if (gameTimer) {
		gameTimer--;
	}
	if (soundTimer) {
		soundTimer--;
	}
}

constexpr bool CPU::isOpcode(opcodeHex_t opcode) noexcept {
	for (auto& results : opcodesAND) {
		if ((opcode & results.first) == static_cast<int>(results.second)) {
			return true;
		}
	}
	return false;
}

CPU::opcodeHex_t CPU::nextOpcode() const {
	const opcodeHex_t opcode{ UCAST((memory[pc] << 8) + memory[pc + 1u]) }; // static_cast to avoid overflow
	ASSERT_MSG(isOpcode(opcode), "Found a bad opcode " + opcodeToStr(opcode) + ", when PC was " + std::to_string(pc) + " (= " + opcodeToStr(pc) + " in hex)", "BAD_OPCODE_FOUND");
	return opcode;
}

void CPU::checkKeys() {
	for (auto& key : keys) {
		key.second = sf::Keyboard::isKeyPressed(static_cast<sf::Keyboard::Key>(key.first));
	}
}

std::string CPU::opcodeToStr(opcodeHex_t opcode, std::size_t finalLength) {
	std::ostringstream ostream{};
	ostream << std::setw(finalLength) << std::setfill('0') << std::hex << std::uppercase << opcode;
	return ostream.str();
}

constexpr bool CPU::isThereOpcodeAfter() const noexcept {
	return std::find_if(memory.cbegin() + pc, memory.cend(), [](byte_t opcode) {return opcode != 0; /* 0 because memory is filled with 0s if the game file is < than memoryUsableSize */ }) != memory.cend();
}

void CPU::initializeFonts() {
	// see here for further informations : http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#font
	const auto writeFont{
		[this](std::size_t number, std::array<address_t, fontSize>&& values) {
			number *= fontSize; // if 1, writes from index 5, 10 for 2, 15 for 3 and so on
			for (auto&& value : values) {
				memory[number++] = static_cast<byte_t>(value);
			}
		}
	};
	/*
	11110000 -> 0xF0
	10010000 -> 0x90
	10010000 -> 0x90
	10010000 -> 0x90
	11110000 -> 0xF0
	*/
	writeFont(0x0, { 0xF0, 0x90, 0x90, 0x90, 0xF0 });
	writeFont(0x1, { 0x20, 0x60, 0x20, 0x20, 0x70 });
	writeFont(0x2, { 0xF0, 0x10, 0xF0, 0x80, 0xF0 });
	writeFont(0x3, { 0xF0, 0x10, 0xF0, 0x10, 0xF0 });
	writeFont(0x4, { 0x90, 0x90, 0xF0, 0x10, 0xF0 });
	writeFont(0x5, { 0xF0, 0x80, 0xF0, 0x10, 0xF0 });
	writeFont(0x6, { 0xF0, 0x80, 0xF0, 0x90, 0xF0 });
	writeFont(0x7, { 0xF0, 0x10, 0x20, 0x40, 0x40 });
	writeFont(0x8, { 0xF0, 0x90, 0xF0, 0x90, 0xF0 });
	writeFont(0x9, { 0xF0, 0x90, 0xF0, 0x10, 0xF0 });
	writeFont(0xA, { 0xF0, 0x90, 0xF0, 0x90, 0x90 });
	writeFont(0xB, { 0xE0, 0x90, 0xE0, 0x90, 0xE0 });
	writeFont(0xC, { 0xF0, 0x80, 0x80, 0xF0, 0xF0 });
	writeFont(0xD, { 0xE0, 0x90, 0x90, 0x90, 0xE0 });
	writeFont(0xE, { 0xF0, 0x80, 0xF0, 0x80, 0xF0 });
	writeFont(0xF, { 0xF0, 0x80, 0xF0, 0x80, 0x80 });
}

CPU::Chip8Key CPU::byteToChip8Key(byte_t keycode) {
	using enum Chip8Key;
	try {
		return std::unordered_map<byte_t, Chip8Key>{
			{ BCAST(0x0), _0 },
			{ BCAST(0x1), _1 },
			{ BCAST(0x2), _2 },
			{ BCAST(0x3), _3 },
			{ BCAST(0x4), _4 },
			{ BCAST(0x5), _5 },
			{ BCAST(0x6), _6 },
			{ BCAST(0x7), _7 },
			{ BCAST(0x8), _8 },
			{ BCAST(0x9), _9 },
			{ BCAST(0xA), _A },
			{ BCAST(0xB), _B },
			{ BCAST(0xC), _C },
			{ BCAST(0xD), _D },
			{ BCAST(0xE), _E },
			{ BCAST(0xF), _F },
		}.at(keycode);
	}
	catch (std::out_of_range&) {
		ASSERT_MSG(false, "Cannot find key corresponding at byte " + opcodeToStr(keycode) + ", value stored in a V register. Check opcodes EX9E, EXA1 !", "BAD_KEY_IN_VX");
		return _0;
	}
}

char CPU::chip8KeyName(sf::Keyboard::Key key) noexcept {
#define KEYCAST(chip8Key) static_cast<long>(static_cast<sf::Keyboard::Key>(chip8Key))
	using enum Chip8Key;
	static const std::array<long, 0x10> keys{
		KEYCAST(_0),
		KEYCAST(_1),
		KEYCAST(_2),
		KEYCAST(_3),
		KEYCAST(_4),
		KEYCAST(_5),
		KEYCAST(_6),
		KEYCAST(_7),
		KEYCAST(_8),
		KEYCAST(_9),
		KEYCAST(_A),
		KEYCAST(_B),
		KEYCAST(_C),
		KEYCAST(_D),
		KEYCAST(_E),
		KEYCAST(_F)
	};
#undef KEYCAST
	const auto pos{ std::find(keys.cbegin(), keys.cend(), static_cast<long>(key)) - keys.cend() };
	if (pos < 10u) { // digit
		return static_cast<char>('0' + pos);
	}
	if (pos < 0x10) { // letter
		return static_cast<char>('A' + pos);
	}
	// bad key param
	return '?';
}

void CPU::interpretOpcode(opcodeHex_t opcode) {
	const auto toOpcode{
		[opcode] {
			for (auto& results : opcodesAND) {
				const bool is0NNN{ results == opcodesAND[0] };
				if ((opcode & results.first) == static_cast<int>(results.second)) {
					if (is0NNN && (opcode & opcodesAND[1].first) != static_cast<int>(opcodesAND[1].second) && (opcode & opcodesAND[2].first) != static_cast<int>(opcodesAND[2].second)) {
						return results.second; // opcode is 0NNN if and only if opcode != 00EE or 00EE
					}
					if (!is0NNN) {
						return results.second;
					}
				}
			}
			ASSERT_MSG(false, "Unexpected execution path in void CPU::interpretOpcode::lambda toOpcode with opcode \"" + opcodeToStr(opcode) + "\"", "UNKNOWN_OPCODE");
			return Opcode::_0NNN; // 0NNN does nothing
		}
	};

	const std::array<byte_t, 4> bits{
			BCAST((opcode & mask_t{0xF000}) >> 12),
			BCAST((opcode & mask_t{0x0F00}) >> 8),
			BCAST((opcode & mask_t{0x00F0}) >> 4),
			BCAST( opcode & mask_t{0x000F})
	};

	const byte_t	  X{ bits[1] };
	const byte_t	  Y{ bits[2] };
	const byte_t	  N{ bits[3] };
	const byte_t     NN{ BCAST((Y << 4) | N) };
	const address_t NNN{ UCAST((X << 8) | NN) };

#ifndef NDEBUG
#define		 X_DEBUG	opcodeToStr(UCAST(X),    1u)
#define		VX_DEBUG	opcodeToStr(UCAST(V[X]), 2u)
#define		 Y_DEBUG	opcodeToStr(UCAST(Y),    1u)
#define		 N_DEBUG	opcodeToStr(UCAST(N),    1u)
#define		NN_DEBUG	opcodeToStr(UCAST(NN),   2u)
#define	   NNN_DEBUG	opcodeToStr(UCAST(NNN),  3u)
#define		PC_DEBUG	opcodeToStr(UCAST(pc)      )
#define		 I_DEBUG	opcodeToStr(UCAST(I)       )
#define		OP_DEBUG	opcodeToStr(UCAST((memory[pc] << 8) | memory[pc + 1u]))
#define NEXTOP_DEBUG	opcodeToStr(UCAST((memory[pc + 2u] << 8) | memory[pc + 3u]))
#else
#define		 X_DEBUG	""
#define		VX_DEBUG	""
#define		 Y_DEBUG	""
#define		 N_DEBUG	""
#define		NN_DEBUG	""
#define	   NNN_DEBUG	""
#define		PC_DEBUG	""
#define		 I_DEBUG	""
#define		OP_DEBUG	""
#define NEXTOP_DEBUG	""
#endif

	switch (toOpcode()) {
		using enum Opcode;

	case _0NNN:
		// not used nor needed
		DEBUG("Historically, called RCA 1082's routine at address " + NNN_DEBUG + " (Not used in modern implementations as this)");
		break;

	case _00E0:
		screen.erase();
		DEBUG("Screen erased");
		break;

	case _00EE:
		ASSERT_MSG(jumps.size() > 0, "Cannot return from a subroutine because the call stack is empty ! PC was " + std::to_string(pc) + " (= " + opcodeToStr(pc) + " in hex)", "EMPTY_CALL_STACK_SUBROUTINE_RETURN");
		pc = jumps.top();
		DEBUG("Returned from subroutine " + PC_DEBUG);
		jumps.pop();
		break;

	case _1NNN:
	{
		enum class Loop {
			// not in loop
			none,
			// first time in loop
			first,
			// in the loop
			in
		};
		static Loop loop{ Loop::none };
		if (loop == Loop::none) {
			DEBUG("Jumped at address " + NNN_DEBUG);
			if (pc == NNN) {
					loop = Loop::first;
				}
		}
		else if (loop == Loop::first) {
			DEBUG("Address didn't change (still " + PC_DEBUG + "), may result into an infinite loop");
			if (pc == NNN) {
				loop = Loop::in;
			}
		}
		// loop::in not handled to avoid unfinite writing into the log/console
		pc = NNN - pcIncrement;
	}
	break;

	case _2NNN:
		jumps.push(pc);
		pc = NNN - pcIncrement;
		DEBUG("Called subroutine at address " + PC_DEBUG);
		break;

	case _3XNN:
		pc += pcIncrement * (V[X] == NN);
		if (V[X] == NN) {
			DEBUG('V' + X_DEBUG + " == " + NN_DEBUG + ", skips instruction " + OP_DEBUG);
		}
		else {
			DEBUG('V' + X_DEBUG + " != " + NN_DEBUG + ", doesn't skip instruction " + NEXTOP_DEBUG);
		}
		break;

	case _4XNN:
		pc += pcIncrement * (V[X] != NN);
		if (V[X] != NN) {
			DEBUG('V' + X_DEBUG + " != " + NN_DEBUG + ", skips instruction " + OP_DEBUG);
		}
		else {
			DEBUG('V' + X_DEBUG + " == " + NN_DEBUG + ", doesn't skip instruction " + NEXTOP_DEBUG);
		}
		break;

	case _5XY0:
		pc += pcIncrement * (V[X] == V[Y]);
		if (V[X] == NN) {
			DEBUG('V' + X_DEBUG + " == V" + Y_DEBUG + ", skips instruction " + OP_DEBUG);
		}
		else {
			DEBUG('V' + X_DEBUG + " != V" + Y_DEBUG + ", doesn't skip instruction " + NEXTOP_DEBUG);
		}
		break;

	case _6XNN:
		V[X] = BCAST(NN);
		DEBUG('V' + X_DEBUG + " = " + NN_DEBUG);
		break;

	case _7XNN:
		V[X] += BCAST(NN);
		DEBUG('V' + X_DEBUG + " += " + NN_DEBUG + ", is now equal to " + VX_DEBUG);
		break;

	case _8XY0:
		V[X] = V[Y];
		DEBUG('V' + X_DEBUG + " = V" + Y_DEBUG + " (= " + VX_DEBUG + ')');
		break;

	case _8XY1:
		V[X] |= V[Y];
		DEBUG('V' + X_DEBUG + " |= V" + Y_DEBUG + ", is now equal to " + VX_DEBUG);
		break;

	case _8XY2:
		V[X] &= V[Y];
		DEBUG('V' + X_DEBUG + " &= V" + Y_DEBUG + ", is now equal to " + VX_DEBUG);
		break;

	case _8XY3:
		V[X] ^= V[Y];
		DEBUG('V' + X_DEBUG + " ^= V" + Y_DEBUG + ", is now equal to " + VX_DEBUG);
		break;

	case _8XY4:
		V[0xF] = static_cast<unsigned>(V[X]) + V[Y] > 0xFF;
		V[X] += V[Y];
		DEBUG('V' + X_DEBUG + " += V" + Y_DEBUG + ", is now equal to " + VX_DEBUG + ", VF is set to " + opcodeToStr(V[0xF], 2) + " (there " + (V[0xF] ? "was an overflow)" : "wasn't overflow)"));
		break;

	case _8XY5:
		V[0xF] = V[X] > V[Y];
		V[X] -= V[Y];
		DEBUG('V' + X_DEBUG + " -= V" + Y_DEBUG + ", is now equal to " + VX_DEBUG + ", VF is set to " + opcodeToStr(V[0xF], 2) + " (there " + (V[0xF] ? "wasn't overflow (borrow))" : "was an overflow (borrow))"));
		break;

	case _8XY6:
		V[0xF] = UCAST(V[X] & 0x01);
		V[X] >>= 1;
		DEBUG("VF = V" + X_DEBUG + " & 0x01 (= " + opcodeToStr(V[0xF], 2) + "), V" + X_DEBUG + " >>= 1, is now equal to " + VX_DEBUG);
		break;

	case _8XY7:
		V[0xF] = V[X] <= V[Y];
		V[X] = UCAST(V[Y] - V[X]);
		DEBUG('V' + X_DEBUG + " = V" + Y_DEBUG + " - V" + X_DEBUG + ", is now equal to " + VX_DEBUG + ", VF is set to " + opcodeToStr(V[0xF], 2) + " (there " + (V[0xF] ? "wasn't overflow(borrow))" : "was an overflow(borrow))"));
		break;

	case _8XYE:
		V[0xF] = UCAST(V[X] >> 7);
		V[X] <<= 1;
		DEBUG("VF = V" + X_DEBUG + " >> 7 (= " + opcodeToStr(V[0xF], 2) + "), V" + X_DEBUG + " <<= 1, is now equal to " + VX_DEBUG);
		break;

	case _9XY0:
		pc += pcIncrement * (V[X] != V[Y]);
		if (V[X] != NN) {
			DEBUG('V' + X_DEBUG + " != V" + Y_DEBUG + ", skips instruction " + OP_DEBUG);
		}
		else {
			DEBUG('V' + X_DEBUG + " == V" + Y_DEBUG + ", doesn't skip instruction " + NEXTOP_DEBUG);
		}
		break;

	case _ANNN:
		I = NNN;
		DEBUG("I = " + NNN_DEBUG);
		break;

	case _BNNN:
		pc = V[0x0] + NNN - pcIncrement;
		DEBUG("PC = V0 + " + NNN_DEBUG + ", now is equal to " + PC_DEBUG);
		break;

	case _CXNN:
		V[X] = BCAST(randomNumber(NN));
		DEBUG('V' + X_DEBUG + " = random number < " + NN_DEBUG + ", nom is equal to " + VX_DEBUG);
		break;

	case _DXYN:
		V[0xF] = 0;
		for (std::size_t lineIndex{ 0u }; lineIndex < N; lineIndex++) {
			std::size_t screenIndex{ Screen::coordsToIndex(V[X], V[Y] + lineIndex) };
			const byte_t lineCode{ memory[I + lineIndex] };
			for (auto charBit : std::bitset<8>(lineCode).to_string()) {
				const bool bit{ charBit == '1' };
				if (bit && screenIndex < Screen::size) { // out of bounds; because std::array::operator[] is noexcept, cannot catch a out-of-range exception
					if (screen[screenIndex].isWhite() && !V[0xF]) { // saves collision once
						V[0xF] = 1;
					}
					screen[screenIndex].flip();
				}
				screenIndex++;
			}
		}
		DEBUG("Displayed font from address I = " + I_DEBUG + ", at coords (" + VX_DEBUG + " ; " + opcodeToStr(V[Y], 2) + "), with height = " + N_DEBUG);
		break;

	case _EXA1:
		pc += pcIncrement * !keys[byteToChip8Key(V[X])];
		if (!keys[byteToChip8Key(V[X])]) {
			DEBUG("Key in V" + X_DEBUG + " (= " + VX_DEBUG + ") isn't pressed, skips instruction " + OP_DEBUG);
		}
		else {
			DEBUG("Key in V" + X_DEBUG + " (= " + VX_DEBUG + ") is pressed, doesn't skip instruction " + NEXTOP_DEBUG);
		}
		break;

	case _EX9E:
		pc += pcIncrement * keys[byteToChip8Key(V[X])];
		if (keys[byteToChip8Key(V[X])]) {
			DEBUG("Key in V" + X_DEBUG + " (= " + VX_DEBUG + ") is pressed, skips instruction " + OP_DEBUG);
		}
		else {
			DEBUG("Key in V" + X_DEBUG + " (= " + VX_DEBUG + ") isn't pressed, doesn't skip instruction " + NEXTOP_DEBUG);
		}
		break;

	case _FX07:
		V[X] = gameTimer;
		DEBUG('V' + X_DEBUG + " = gameTimer, is now equal to " + opcodeToStr(gameTimer, 2));
		break;

	case _FX0A:
	{
		sf::Event event{};
		while (window.isOpen()) {
			if (!window.pollEvent(event)) { // event is default-initialzed to 0 (= sf::Event::Closed), so if pollEvent doesn't detect any event, the variable event stays to closed and the loop is exited
				continue;
			}
			PAUSE_IF_NOT_FOCUS(ROM_);
			if (event.type == sf::Event::Closed) {
				window.close();
				END_PROGRAM_ERR("User closes the emulator", ROM_, "USER_CLOSE");
				continue;
			}
			else if (event.type == sf::Event::KeyPressed) {
				const auto keyName{ chip8KeyName(event.key.code) };
				if (keyName != '?') { // key is one of the Chip8 keyboard
					V[X] = std::isdigit(keyName) ? keyName - '0' : keyName - 'A' + 10u; // + 10 because A will result into 0 (A - A), so we must count the 10 digits (A - A + 10 = 10, hex value of A)
					keys[static_cast<Chip8Key>(V[X])] = true;
					using namespace std::string_literals;
					DEBUG("Key "s + keyName + " is pressed");
					break;
				}
			}
		}
		break;
	}

	case _FX15:
		gameTimer = V[X];
		DEBUG("gameTimer = V" + X_DEBUG + ", is now equal to " + opcodeToStr(gameTimer, 2));
		break;

	case _FX18:
		soundTimer = V[X];
		DEBUG("gameTimer = V" + X_DEBUG + ", is now equal to " + opcodeToStr(gameTimer, 2));
		break;

	case _FX1E:
		V[0xF] = static_cast<unsigned long>(V[X]) + I > 0xFFF;
		I += V[X];
		DEBUG("I += V" + X_DEBUG + ", is now equal to " + I_DEBUG);
		break;

	case _FX29:
		I = fontSize * V[X]; // because fonts start at address 0
		DEBUG("I = address of font in V" + X_DEBUG + " (= " + VX_DEBUG + "), is now equal to " + I_DEBUG);
		break;

	case _FX33:
		memory[I] = BCAST(V[X] / 100);
		memory[I + 1u] = BCAST((V[X] % 100) / 10);
		memory[I + 2u] = BCAST(V[X] % 10);
		DEBUG("address " + I_DEBUG + " of memory = " + opcodeToStr(memory[I]) +
			"\naddress " + opcodeToStr(I + 1u) + " of memory = " + opcodeToStr(memory[I + 1u]) +
			"\naddress " + opcodeToStr(I + 2u) + " of memory = " + opcodeToStr(memory[I + 2u]));
		break;

	case _FX55:
	{
		std::string debugText{};
		for (address_t i{ 0u }; i <= X; i++) {
			memory[UCAST(I + i)] = V[i];
			if (i > 0) {
				debugText += "\n";
			}
			debugText += "address " + opcodeToStr(UCAST(I + i)) + " of memory = " + opcodeToStr(memory[UCAST(I + i)]);
		}
		DEBUG(debugText);
		break;
	}

	case _FX65:
	{
		std::string debugText{};
		for (address_t i{ 0 }; i <= X; i++) {
			V[i] = memory[UCAST(I + i)];
			if (i > 0) {
				debugText += ";\t";
			}
			debugText += 'V' + opcodeToStr(i, 1) + " is loaded from memory at address " + opcodeToStr(UCAST(I + i)) + ", is now equal to " + opcodeToStr(V[i]);
		}
		DEBUG(debugText);
		break;
	}

	}
	pc += pcIncrement;
}

bool CPU::loadGame(const std::string& ROM) {
	const std::wstring extension{ std::filesystem::path(ROM).extension().native() };
	if (extension != L".ch8") {
		std::cerr << "Warning : file " << std::quoted(ROM) << " hasn't standard extension .ch8 !" << std::endl <<
			"It may not be an usable Chip8 ROM file and may result into errors !" << std::endl <<
			"If the file's correct, you should rename it to explicitly show that's a good file !" << std::endl;
		char c{ '0' };
		while (c != 'y' && c != 'n') {
			std::cout << "Are you sure you want to continue (y/n) ?" << std::endl;
			std::cin >> c;
		}
		if (c == 'n') {
			std::cerr << "Loading aborted : USER_EXIT_WARNING" << std::endl;
			return false;
		}
	}

	ROM_ = ROM;

	using uifstream = std::basic_ifstream<byte_t>;

	uifstream ROMfile{ ROM, std::ios_base::binary };
	if (!ROMfile) {
		END_PROGRAM_ERR("File's loading failed", ROM, "FILE_NOT_FOUND !");
		return false;
	}
	const auto ROMsize{ std::filesystem::file_size(ROM) };
	if (ROMsize > memoryUsableSize) {
		END_PROGRAM_ERR("File too big (" + std::to_string(ROMsize) + " bytes whereas max allowed size is " + std::to_string(memoryUsableSize) + " bytes)", ROM, "FILE_TOO_BIG");
		std::cerr << "File too big (" << ROMsize << " bytes, max capacity is " << memoryUsableSize << ") !" << std::endl;
		return false;
	}
	ROMfile.read(&memory[memoryStart], static_cast<std::streamsize>(ROMsize));
	return true;
}

void CPU::emulate(const std::string& ROMpath) {
	ROM_ = ROMpath;
	if (!loadGame(ROMpath)) {
		END_PROGRAM_ERR("File cannot be opened", ROMpath, "FILE_NOT_FOUND");
		return;
	}
	static sf::SoundBuffer beepBuf{};
	if (!beepBuf.loadFromFile("beep.wav")) {
		WARNING("Sound beep.wav cannot be loaded", "SOUND_NOT_LOADED");
	}
	static sf::Sound beep{ beepBuf };
	beep.setVolume(50.f);
	const std::string ROMname{ ROM_NAME(ROMpath) };
	window.setTitle(ROMname);
	while (window.isOpen()) {
		beep.stop();
		QUIT_IF_NOTHING_TO_EMULATE
		if (soundTimer > 0) {
			beep.play();
		}
		sf::Event event;
		if (window.pollEvent(event)) {
			PAUSE_IF_NOT_FOCUS(ROMname);
			if (event.type == sf::Event::Closed) {
				window.close();
				END_PROGRAM_ERR("User closes the emulator", ROMname, "USER_CLOSE");
				continue;
			}
		}
		if (event.type == sf::Event::KeyPressed || event.type == sf::Event::KeyReleased) {
			if (chip8KeyName(event.key.code) != '?') {
				keys[static_cast<Chip8Key>(event.key.code)] = event.type == sf::Event::KeyPressed;
			}
		}
		for (auto opcode{ 0u }; opcode < opcodesPerFrame; opcode++) {
			QUIT_IF_NOTHING_TO_EMULATE
			interpretOpcode(nextOpcode());
		}
		window.clear(sf::Color::Black);
		window.draw(screen);
		window.display();
		count();
		sf::sleep(sf::Time(sf::seconds(delay)));
	}
}

CPU::CPU() {
	initializeFonts();
	// inits the log file
#ifdef DEBUG_FILE
	if (std::filesystem::exists("results.log")) {
		if (!std::filesystem::remove("results.log")) {
			ERROR("Cannot remove file \"results.log\". This session's log will be added to previous log.\n"
				"To resolve this error, you should check where is the current directory, so you must run the program as administrator to write there.", "REMOVE_LOG_FAIL");
		}
	}
#endif
}