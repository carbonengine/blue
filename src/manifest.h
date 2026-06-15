// Copyright © 2014 CCP ehf.

#pragma once

typedef std::vector<std::string> directives_t;

Be::Result<std::string> VerifyManifestFile( const std::string& name, directives_t& directives );