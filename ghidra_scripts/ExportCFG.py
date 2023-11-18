#TODO write a description for this script
#@author 
#@category CS5231
#@keybinding 
#@menupath 
#@toolbar 

from ghidra.program.model.block import BasicBlockModel
import sys
import os

args = getScriptArgs()
if len(args) > 1:
	print('[' + getScriptName() + '] ' + 'Invalid Parameters. Usage: ./analyzeHeadless ... ' + getScriptName() + ' <OUTPUT_FILENAME>')
	sys.exit(1)

cfg = {}

functionManager = currentProgram.getFunctionManager()
referenceManager = currentProgram.getReferenceManager()

baseAddress = currentProgram.getMinAddress()

codeBlockiterator = BasicBlockModel(currentProgram).getCodeBlocks(monitor)
while codeBlockiterator.hasNext():
	codeBlock = codeBlockiterator.next()

	destinationIterator = codeBlock.getDestinations(monitor)
	while destinationIterator.hasNext():
		destinationBlock = destinationIterator.next()
		flowType = destinationBlock.getFlowType()
		if not flowType.isComputed():
			continue

		dest_addr = destinationBlock.getDestinationAddress()
		dest_function = functionManager.getFunctionAt(dest_addr)
		if dest_function and currentProgram.getMemory().isExternalBlockAddress(dest_addr):
			destination = dest_function.getName()

			thunked_function = dest_function.getThunkedFunction(False)
			if thunked_function is not None:
				library_name = thunked_function.getExternalLocation().getLibraryName()
				if library_name != '<EXTERNAL>':
					destination = library_name + '::' + destination
		else:
			destination = int(dest_addr.getOffset() - baseAddress.getOffset())

		src_addr = hex(int(destinationBlock.getReferent().getOffset() - baseAddress.getOffset()))
		if src_addr in cfg:
			cfg[src_addr].add(destination)
		else:
			cfg[src_addr] = set([destination])

output = ''
for key, value in cfg.items():
	output += key + ' '
	for i, item in enumerate(value):
		if i > 0:
			output += ','

		if isinstance(item, (int, long)):
			output += ('O:' + hex(item))
			continue

		output += ('S:' + item)

	output += '\n'

if len(args) <= 0:
	print('[' + getScriptName() + ']\n' + output)
	sys.exit(0)

filename = os.path.normpath(os.path.abspath(args[0]))
# if os.path.exists(filename):
# 	print('[' + getScriptName() + '] File or folder with the same name already exists - \"' + filename + '\"')
# 	sys.exit(1)

with open(filename, 'w') as file:
	file.write(output)

print('[' + getScriptName() + ']' + ' Output filename - \"' + filename + '\"')
