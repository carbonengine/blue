#This is an import library mangler for ruby.
#Creates a Borland C import library from a .dll written in MSDev
#basically, it's just a hack that generates underscore aliases for function names.
def Filter(input, output)
	mangled = /^\s*\?/
	function = /^(\s*)(\w+)\s+@\d+/
	input.each_line do |line|
		next if line =~ mangled
		if line =~ function
			fname = $2
			output.puts  "#{$1}_#{fname} = #{fname}"
			next
		end
		output.puts line
	end
end

importlib = ARGV[0]
dll = ARGV[1]

tmpname = "tempfile.def"
tmpname2 = "tempfile2.def"

system("impdef %s %s"% [tmpname, dll])

infile = File.new(tmpname)
outfile = File.new(tmpname2, "w")
Filter(infile, outfile)
infile.close
outfile.close
File.delete(tmpname)

system("implib %s %s"%[importlib, tmpname2])
File.delete(tmpname2)