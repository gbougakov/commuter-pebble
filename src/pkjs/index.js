// NMBS Pebble App - Main Entry Point
// This file coordinates all modules and initializes the application

// Require all modules in dependency order
// config-manager.js registers all Pebble event listeners when loaded
require('./05-config-manager.js');

// All modules are loaded and Pebble event listeners are registered automatically
console.log('NMBS Pebble App modules loaded');
