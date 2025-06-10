import gdb
import argparse

class AddAssemblySymbols(gdb.Command):
	"""Explore loaded_assemblies linked list and add ELF section symbols."""

	def __init__(self):
		super(AddAssemblySymbols, self).__init__("add-assembly-symbols", gdb.COMMAND_USER)

	def invoke(self, arg, from_tty):
		loaded_assemblies_sym = "loaded_assemblies"

		'''
		# Save enabled breakpoint IDs
		enabled_breakpoints = []
		for bp in gdb.breakpoints():
			if bp.enabled:
				enabled_breakpoints.append(bp.number)
		
		# Print the IDs of enabled breakpoints
		if enabled_breakpoints:
			print(f"Enabled breakpoints: {', '.join(map(str, enabled_breakpoints))}")
		else:
			print("No enabled breakpoints found.")
		'''

		try:
			# Parse arguments to detect -b flag
			parser = argparse.ArgumentParser()
			parser.add_argument("-b", "--bootloader", action="store_true")
			args = parser.parse_args(arg.split())

			loaded_assemblies = gdb.parse_and_eval(loaded_assemblies_sym)
			if args.bootloader:
				print("Resetting symbol table (and reloading bootloader ELF)...")
				gdb.execute("file build/kernel8.elf", to_string=False)
		except gdb.error:
			print(f"Symbol '{loaded_assemblies_sym}' not found.")
			return
		except Exception as e:
			print(e)
		except SystemExit:
			return

		node = loaded_assemblies
		idx = 0
		while node and int(node):
			try:

				# Dereference the node to get the assembly structure
				assembly = node.dereference()
				if not assembly['name']:
					print(f"Assembly {idx}: No name found, skipping.")
					node = assembly['next']
					idx += 1
					continue

				# Extract the ELF path and section information
				elf_path = str(assembly['name'].string())
				sections = assembly['sections']
				section_node = sections
				section_addrs = {}
				# Walk section_data linked list
				while section_node and int(section_node):
					section = section_node.dereference()
					name = str(section['name'].string())
					addr = int(section['address'])
					section_addrs[name] = addr
					section_node = section['next']

				if '.text' not in section_addrs:
					print(f"Assembly {idx}: {elf_path} has no .text section, skipping.")
					node = assembly['next']
					idx += 1
					continue

				cmd = f"add-symbol-file {elf_path} {section_addrs['.text']}"
				for name, addr in section_addrs.items():
					if name == '.text':
						continue
					cmd += f" -s {name} {addr}"

				print(f"Assembly {idx}: {elf_path}")
				print(f"Running: {cmd}")
				gdb.execute(cmd, to_string=False)

				node = assembly['next']
				idx += 1
			except Exception as e:
				print(f"Error processing assembly: {e}")
				break

		'''
		# # Disable all breakpoints
		# gdb.execute("disable breakpoints", to_string=False)

		# # Re-enable previously enabled breakpoints
		# for bp_num in enabled_breakpoints:
		# 	gdb.execute(f"enable {bp_num}", to_string=False)
		'''

		if not args.bootloader:
			print("Resetting symbol table...")
			gdb.execute("file -readnever build/kernel8.elf", to_string=False)

AddAssemblySymbols()