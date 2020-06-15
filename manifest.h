////////////////////////////////////////////////////////////////////////////////
//
// Creator:   Kristjan Gerhardsson
// Created:   May 2020
// Copyright: CCP 2020
//

#pragma once

typedef std::vector<std::string> directives_t;

Be::Result<std::string> VerifyManifestFile( const std::string& name, directives_t& directives );